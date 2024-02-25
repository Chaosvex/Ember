/*
 * Copyright (c) 2014 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Parser.h"
#include "Generator.h"
#include "DBCGenerator.h"
#include "SQLDDLGenerator.h"
#include "SQLDMLGenerator.h"
#include "Validator.h"
#include "bprinter/table_printer.h"
#include <logger/Logging.h>
#include <logger/ConsoleSink.h>
#include <logger/FileSink.h>
#include <boost/program_options.hpp>
#include <boost/bind.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <unordered_map>

namespace po = boost::program_options;
namespace fs = std::filesystem;
namespace edbc = ember::dbc;
namespace el = ember::log;

int launch(const po::variables_map& args);
po::variables_map parse_arguments(int argc, const char* argv[]);
std::vector<std::string> fetch_definitions(std::span<const std::string> paths);
void print_dbc_table(const edbc::types::Definitions& defs);
void print_dbc_fields(const edbc::types::Definitions& defs);
void handle_options(const po::variables_map& args, const edbc::types::Definitions& defs);

int main(int argc, const char* argv[]) try {
	const po::variables_map args = parse_arguments(argc, argv);
	auto con_verbosity = el::severity_string(args["verbosity"].as<std::string>());
	auto file_verbosity = el::severity_string(args["fverbosity"].as<std::string>());

	auto logger = std::make_unique<el::Logger>();
	auto fsink = std::make_unique<el::FileSink>(file_verbosity, el::Filter(0),
	                                            "dbcparser.log", el::FileSink::Mode::APPEND);
	auto consink = std::make_unique<el::ConsoleSink>(con_verbosity, el::Filter(0));
	consink->colourise(true);
	logger->add_sink(std::move(consink));
	logger->add_sink(std::move(fsink));
	el::set_global_logger(logger.get());

	return launch(args);
} catch(const std::exception& e) {
	std::cerr << e.what();
	return 1;
}

int launch(const po::variables_map& args) try {
	const auto def_paths = args["definitions"].as<std::vector<std::string>>();

	std::vector<std::string> paths = fetch_definitions(def_paths);

	edbc::Parser parser;
	auto definitions = parser.parse(paths);
	handle_options(args, definitions);
	return 0;
} catch(const std::exception& e) {
	LOG_FATAL_GLOB << e.what() << LOG_SYNC;
	return 1;
}

void handle_options(const po::variables_map& args, const edbc::types::Definitions& defs) {
	edbc::Validator validator;
	edbc::Validator::Options val_opts { edbc::Validator::VAL_SKIP_FOREIGN_KEYS };

	// if we're doing code generation for a DBC that references other DBCs, we
	// need to make sure that those references are also valid, otherwise we
	// might generate code that doesn't compile
	if(args["disk"].as<bool>()) {
		val_opts = static_cast<edbc::Validator::Options>(val_opts & ~edbc::Validator::VAL_SKIP_FOREIGN_KEYS);
	}

	validator.validate(defs, val_opts);

	if(args["print-dbcs"].as<bool>()) {
		print_dbc_table(defs);
		return;
	}

	if(args["print-fields"].as<bool>()) {
		print_dbc_fields(defs);
		return;
	}

	const auto& out = args["output"].as<std::string>();

	if(args["dbc-gen"].as<bool>()) {
		for(const auto& dbc : defs) {
			if(dbc->type == edbc::types::Type::STRUCT) {
				edbc::generate_dbc_template(static_cast<const edbc::types::Struct*>(dbc.get()), out);
			}
		}
	}

	if(args["disk"].as<bool>()) {
		edbc::generate_common(defs, out, args["templates"].as<std::string>());
		edbc::generate_disk_source(defs, out, args["templates"].as<std::string>());
	}

	if(args["sql-schema"].as<bool>()) {
		edbc::generate_sql_ddl(defs, out);
	}

	if(args["sql-data"].as<bool>()) {
		edbc::generate_sql_dml(defs, out);
	}

	LOG_DEBUG_GLOB << "Done!" << LOG_ASYNC;
}

void print_dbc_table(const edbc::types::Definitions& defs) {
	const auto comment_len = 45;
	const auto name_len = 26;

	bprinter::TablePrinter printer(&std::cout);
	printer.AddColumn("DBC Name", name_len);
	printer.AddColumn("#", 4);
	printer.AddColumn("Comment", comment_len);
	printer.PrintHeader();

	for(auto& def : defs) {
		if(def->type == edbc::types::Type::STRUCT) {
			auto dbc = static_cast<const edbc::types::Struct*>(def.get());
			printer << std::string_view(dbc->name).substr(0, name_len) << dbc->fields.size()
				<< dbc->comment;
		}
	}

	printer.PrintFooter();
}

void print_dbc_fields(const edbc::types::Definitions& groups) {
	for(const auto& def : groups) {
		std::cout << def->name << "\n";

		bprinter::TablePrinter printer(&std::cout);
		printer.AddColumn("Field", 32);
		printer.AddColumn("Type", 18);
		printer.AddColumn("Key", 4);
		printer.AddColumn("Comment", 20);
		printer.PrintHeader();

		if(def->type != edbc::types::Type::STRUCT) {
			continue;
		}

		const auto dbc = static_cast<const edbc::types::Struct*>(def.get());

		for(const auto& f : dbc->fields) {
			std::string key;

			switch(f.keys.size()) {
				case 1:
					key = f.keys[0].type.data()[0];
					break;
				case 2:
					key = "pf";
					break;
			}

			printer << f.name << f.underlying_type << key << f.comment;
		}

		printer.PrintFooter();
	}
}

std::vector<std::string> fetch_definitions(std::span<const std::string> paths) {
	std::vector<std::string> xml_paths;

	for(const std::filesystem::path& path : paths) {
		if(std::filesystem::is_directory(path)) {
			for(auto& file : fs::directory_iterator(path)) {
				if(file.path().extension() == ".xml") {
					xml_paths.emplace_back(file.path().string());
				}
			}
		} else if(std::filesystem::is_regular_file(path) && path.extension() == ".xml") {
			xml_paths.emplace_back(path.string());
		} else {
			throw std::invalid_argument("Invalid directory or DBC path provided, " + path.string());
		}
	}

	return xml_paths;
}

po::variables_map parse_arguments(int argc, const char* argv[]) {
	po::options_description opt("Options");
	opt.add_options()
		("help,h", "Displays a list of available options")
		("definitions,d", po::value<std::vector<std::string>>()->multitoken()->default_value({"/"}, "/"),
			"Path to a directory containing DBC definitions or a specific DBC definition. "
			"Multiple paths may be specified but there should be no overlap of DBC definitions.")
		("output,o", po::value<std::string>()->default_value(""),
			"Directory to save output to")
		("templates,t", po::value<std::string>()->default_value("templates/"),
			"Path to the code generation templates")
		("verbosity,v", po::value<std::string>()->default_value("info"),
			"Logging verbosity")
		("fverbosity", po::value<std::string>()->default_value("disabled"),
			"File logging verbosity")
		("disk", po::bool_switch(),
			"Generate files required for loading DBC data from disk")
		("print-dbcs", po::bool_switch(),
			"Print out a summary of the DBC definitions in a table")
		("print-fields", po::bool_switch(),
			"Print out of a summary of the loaded DBC definitions")
		("dbc-gen", po::bool_switch(),
			"Generate empty DBC files for editing in other tools")
		("sql-schema", po::bool_switch(),
			"Generate SQL DDL from DBC schemas")
		("sql-data", po::bool_switch(),
			"Generate SQL DML from DBC files");

	po::variables_map options;
	po::store(po::command_line_parser(argc, argv).options(opt)
	          .style(po::command_line_style::default_style & ~po::command_line_style::allow_guessing)
	          .run(), options);

	if(options.count("help") || argc <= 1) {
		std::cout << opt << "\n";
		std::exit(0);
	}

	po::notify(options);

	return options;
}