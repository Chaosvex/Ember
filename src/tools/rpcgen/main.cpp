/*
 * Copyright (c) 2021 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "SchemaParser.h"
#include <logger/Logger.h>
#include <logger/ConsoleSink.h>
#include <logger/FileSink.h>
#include <boost/program_options.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdlib>

using namespace ember;
namespace po = boost::program_options;

int launch(const po::variables_map& args);
void configure_logger(log::Logger& logger, const po::variables_map& args);
po::variables_map parse_arguments(int argc, const char* argv[]);

int main(int argc, const char* argv[]) try {
	const po::variables_map args = parse_arguments(argc, argv);
	log::Logger logger;
	configure_logger(logger, args);
	return launch(args);
} catch(const std::exception& e) {
	std::cerr << e.what();
	return EXIT_FAILURE;
}

int launch(const po::variables_map& args) try {
	const auto& out_path = args["out"].as<std::string>();
	const auto& tpl_path = args["tpl"].as<std::string>();
	const auto& schemas = args["schemas"].as<std::vector<std::string>>();

	ember::SchemaParser parser(tpl_path, out_path);

	for(const auto& schema : schemas) {
		LOG_INFO_GLOB << "Generating service for " << schema << LOG_SYNC;
		parser.generate(schema);
	}

	return EXIT_SUCCESS;
} catch (const std::exception& e) {
	LOG_FATAL_GLOB << e.what() << LOG_SYNC;
	return EXIT_FAILURE;
}

void configure_logger(log::Logger& logger, const po::variables_map& args) {
	const auto& con_verbosity = log::severity_string(args["verbosity"].as<std::string>());
	const auto& file_verbosity = log::severity_string(args["fverbosity"].as<std::string>());
	
	auto fsink = std::make_unique<log::FileSink>(
		file_verbosity, log::Filter(0), "rpcgen.log", log::FileSink::Mode::APPEND
	);

	auto consink = std::make_unique<log::ConsoleSink>(con_verbosity, log::Filter(0));
	consink->colourise(true);
	logger->add_sink(std::move(consink));
	logger->add_sink(std::move(fsink));
	log::global_logger(logger);
}

po::variables_map parse_arguments(int argc, const char* argv[]) {
	po::options_description cmdline_opts("Options");
	cmdline_opts.add_options()
		("schemas,s", po::value<std::vector<std::string>>()->multitoken()->required(), ".fbsb schemas")
		("out,o",     po::value<std::string>()->required(), "Output directory for generated code")
		("tpl,t",     po::value<std::string>()->default_value("templates/"),
			"Path to the templates")
		("verbosity,v", po::value<std::string>()->default_value("info"),
			"Logging verbosity")
		("fverbosity", po::value<std::string>()->default_value("disabled"),
			"File logging verbosity");

	po::variables_map options;
	po::store(po::command_line_parser(argc, argv).options(cmdline_opts).run(), options);

	if(options.count("help") || argc <= 1) {
		std::cout << cmdline_opts;
		std::exit(EXIT_SUCCESS);
	}

	po::notify(options);

	return options;
}