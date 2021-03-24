/*
 * Copyright (c) 2019 - 2021 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "DatabaseDetails.h"
#include "QueryExecutor.h"
#ifdef DB_MYSQL
	#include "mysql/MySQLQueryExecutor.h"
#elif DB_POSTGRESQL
 	#include "postgresql/PostgreSQLQueryExecutor.h"
#endif
#include <logger/Logging.h>
#include <logger/ConsoleSink.h>
#include <logger/FileSink.h>
#include <shared/Version.h>
#include <boost/program_options.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <algorithm>
#include <chrono>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <regex>
#include <thread>
#include <unordered_map>
#include <filesystem>
#include <vector>
#include <utility>

using namespace ember;

namespace po = boost::program_options;
namespace el = ember::log;

const std::unordered_map<std::string_view, const std::vector<std::string_view>> db_args {
	{ "login", { "login.root-user", "login.root-password" }},
	{ "world", { "world.root-user", "world.root-password" }}
};

const auto UPDATE_BACKOUT_PERIOD = std::chrono::seconds(5);

int launch(const po::variables_map& args);
po::variables_map parse_arguments(int argc, const char* argv[]);
void validate_options(const po::variables_map& args);
void validate_db_args(const po::variables_map& args, const std::string& mode);
void db_install(const po::variables_map& args, const std::string& db_name, const bool drop);
bool db_update(const po::variables_map& args, const std::string& db_name);
DatabaseDetails db_details(const po::variables_map& args, const std::string& db);
bool apply_updates(const po::variables_map& args, QueryExecutor& exec,
                   std::vector<std::string> migration_paths, const std::string& db);

int main(int argc, const char* argv[]) try {
	std::cout << "Build " << ember::version::VERSION << " (" << ember::version::GIT_HASH << ")\n";
	const po::variables_map args = parse_arguments(argc, argv);
	auto con_verbosity = el::severity_string(args["verbosity"].as<std::string>());
	auto file_verbosity = el::severity_string(args["fverbosity"].as<std::string>());

	auto logger = std::make_unique<el::Logger>();

	auto fsink = std::make_unique<el::FileSink>(
		file_verbosity, el::Filter(0), "dbmanage.log", el::FileSink::Mode::APPEND
	);

	auto consink = std::make_unique<el::ConsoleSink>(con_verbosity, el::Filter(0));
	consink->colourise(true);
	logger->add_sink(std::move(consink));
	logger->add_sink(std::move(fsink));
	el::set_global_logger(logger.get());

	return launch(args);
} catch(const std::exception& e) {
	std::cerr << e.what() << "\n";
	return 1;
}

std::unique_ptr<QueryExecutor> db_executor(const std::string& type,
										   const DatabaseDetails& details) {
	auto lower_type = type;
	std::transform(lower_type.begin(), lower_type.end(), lower_type.begin(), ::tolower);

#ifdef DB_MYSQL
	if(type == "mysql") {
		return std::make_unique<MySQLQueryExecutor>(details);
	}
#elif DB_POSTGRESQL
	if(type == "postgresql") {
		throw std::illegal_argument("PostgreSQL is unsupported for the time being.");
	}
#endif

	return nullptr;
}

int launch(const po::variables_map& args) try {
	validate_options(args);

	if(!args["install"].empty()) {
		LOG_INFO_GLOB << "Starting database setup process..." << LOG_SYNC;
		const auto clean = args["clean"].as<bool>();

		if(clean && !args["shutup"].as<bool>()) {
			LOG_WARN_GLOB << "You are performing an installation with --clean.\n"
			              << "This will drop any existing databases and users specified "
			                 "in the arguments!\n"
			                 "Proceeding in " << UPDATE_BACKOUT_PERIOD.count()
			              << " seconds..." << LOG_SYNC;
			std::this_thread::sleep_for(UPDATE_BACKOUT_PERIOD);
		}

		const auto& dbs = args["install"].as<std::vector<std::string>>();

		for(const auto& db : dbs) {
			const auto& details = db_details(args, db);
			auto executor = db_executor(args["database-type"].as<std::string>(), details);

			LOG_INFO_GLOB << "Testing connection " << details.username << "@"
			      << details.hostname << ":" << details.port
			      << " for " << db << LOG_SYNC;

			if(executor->test_connection()) {
				LOG_INFO_GLOB << "Successfully established connection" << LOG_SYNC;
			} else {
				throw std::runtime_error("Unable to establish database connection");
			}
		}
	
		for(const auto& db : dbs) {
			db_install(args, db, clean);
		}

		LOG_INFO_GLOB << "Database installation complete!" << LOG_SYNC;

		if(args["update"].empty()) {
			LOG_INFO_GLOB << "Consider running --update." << LOG_SYNC;
		}
	}

	bool success = true;

	if(!args["update"].empty()) {
		LOG_INFO_GLOB << "Starting database update process..." << LOG_SYNC;

		if(!args["shutup"].as<bool>()) {
			LOG_WARN_GLOB << "Please ensure all running Ember services have been "
			                 "stopped and you have backed up your database!\n"
			                 "Proceeding in " << UPDATE_BACKOUT_PERIOD.count()
			              << " seconds..." << LOG_SYNC;
			std::this_thread::sleep_for(UPDATE_BACKOUT_PERIOD);
		}

		const auto& dbs = args["update"].as<std::vector<std::string>>();

		success = !std::any_of(dbs.begin(), dbs.end(), [&](const auto& db) {
			return !db_update(args, db);
		});
	}

	if(success) {
		LOG_INFO_GLOB << "All operations have completed successfully!" << LOG_SYNC;
	} else {
		LOG_WARN_GLOB << "Some operations did not complete successfully!" << LOG_SYNC;
	}

	return success? 0 : 1;
} catch(const std::exception& e) {
	LOG_FATAL_GLOB << e.what() << LOG_SYNC;
	return 1;
}

void validate_options(const po::variables_map& args) {
	LOG_TRACE_GLOB << __func__ << LOG_SYNC;

	if(args["install"].empty() && args["update"].empty()) {
		throw std::invalid_argument("At least --install or --update must be specified!");
	}

	if(!args["install"].empty()) {
		validate_db_args(args, "install");
	}
	
	if(!args["update"].empty()) {
		validate_db_args(args, "update");
	}
}

bool validate_db_names(const std::vector<std::string>& input_names) {
	std::vector<std::string_view> valid_names;
	std::vector<std::string_view> bad_names;
	std::vector<std::string_view> input(input_names.begin(), input_names.end());

	for(const auto& [key, value] : db_args) {
		valid_names.emplace_back(key);
	}

	std::sort(valid_names.begin(), valid_names.end());
	std::sort(input.begin(), input.end());

	std::set_difference(input.begin(), input.end(),
	                    valid_names.begin(), valid_names.end(),
	                    std::back_inserter(bad_names));
	
	for(const auto& name : bad_names) {
		LOG_INFO_GLOB << "Invalid or duplicate name: " << name << LOG_SYNC;
	}

	return bad_names.empty();
}

void validate_db_args(const po::variables_map& po_args, const std::string& mode) {
	LOG_TRACE_GLOB << __func__ << LOG_SYNC;

	const auto& dbs = po_args[mode].as<std::vector<std::string>>();

	if(!validate_db_names(dbs)) {
		throw std::invalid_argument(
			"Database argument list contained duplicates or unknown names. "
			"Please fix this before attempting to continue."
		);
	}

	// ensure that all arguments required for managing this DB are present
	for(const auto& db_name : dbs) {
		const auto args = db_args.at(db_name);
		const auto missing_arg = std::find_if(args.begin(), args.end(), [&](const auto& arg) {
			return po_args[arg.data()].empty();
		});

		if(missing_arg != args.end()) {
			throw std::invalid_argument(std::string("Missing argument for ") + db_name + ": " + missing_arg->data());
		}
	}
}

std::vector<std::string> load_queries(const std::string& path) {
	LOG_TRACE_GLOB << __func__ << LOG_SYNC;

	std::vector<std::string> queries;
	std::fstream file(path, std::ios::in);

	if(!file) {
		throw std::runtime_error("Unable to open SQL from " + path);
	}

	std::string query;

	while(std::getline(file, query, ';')) {
		queries.emplace_back(std::move(query));
	}

	return queries;
}

DatabaseDetails db_details(const po::variables_map& args, const std::string& db) {
	const auto root_user_arg = std::string(db) + ".root-user";
	const auto root_pass_arg = std::string(db) + ".root-password";
	const auto hostname_arg = std::string(db) + ".hostname";
	const auto port_arg = std::string(db) + ".port";
	const auto& root_user = args[root_user_arg].as<std::string>();
	const auto& root_pass = args[root_pass_arg].as<std::string>();
	const auto& hostname = args[hostname_arg].as<std::string>();
	const auto port = args[port_arg].as<std::uint16_t>();

	return {
		.username = root_user,
		.password = root_pass,
		.hostname = hostname,
		.port = port,
	};
}

void db_install(const po::variables_map& args, const std::string& db, const bool drop) {
	LOG_TRACE_GLOB << __func__ << LOG_SYNC;
	const auto& sql_dir = args["sql-dir"].as<std::string>();
	const auto& dbs = args["install"].as<std::vector<std::string>>();

	// Ensure we can connect to the database before attempting the installation
	const auto& details = db_details(args, db);			
	auto executor = db_executor(args["database-type"].as<std::string>(), details);

	if(!executor) {
		throw std::runtime_error("Unable to obtain a database executor. Invalid database type?");
	}
	
	if(!executor->test_connection()) {
		throw std::runtime_error("Unable to establish database connection");
	}

	// All good? Let's get those databases installed!
	const auto user_arg = std::string(db) + ".set-user";
	const auto pass_arg = std::string(db) + ".set-password";
	const auto db_arg = std::string(db) + ".db-name";

	if(args[user_arg].empty()) {
		throw std::invalid_argument("Missing argument, " + user_arg);
	}

	if(args[pass_arg].empty()) {
		throw std::invalid_argument("Missing argument, " + pass_arg);
	}

	const auto& db_name = args[db_arg].as<std::string>();
	const auto& user = args[user_arg].as<std::string>();
	const auto& pass = args[pass_arg].as<std::string>();

	if (user == executor->details().username) {
		throw std::runtime_error("Privileged DB user and new user cannot match.");
	}

	LOG_INFO_GLOB << "Creating database " << args[db_arg].as<std::string>() << "..." << LOG_SYNC;
	executor->create_database(db_name, drop);
	executor->select_db(db_name);

	LOG_INFO_GLOB << "Installing " << db_name << " schema..." << LOG_SYNC;
	const auto& db_type = args["database-type"].as<std::string>();
	const auto& path = args["sql-dir"].as<std::string>()  + db_type + "/" + db + "/schema.sql";
	const auto& queries = load_queries(path);

	for(const auto& query : queries) {
		executor->execute(query);
	}

	LOG_INFO_GLOB << "Creating user " << user << "..." << LOG_SYNC;
	executor->create_user(user, pass, drop);

	LOG_INFO_GLOB << "Granting " << user << " access to " << db_name << "..." << LOG_SYNC;
	const bool read_only = (db == "world");
	executor->grant_user(user, db_name, read_only);
	LOG_INFO_GLOB << "Successfully installed " << db << LOG_SYNC;
}

bool apply_updates(const po::variables_map& args, QueryExecutor& exec,
                   std::vector<std::string> migration_paths, const std::string& db) {
	LOG_TRACE_GLOB << __func__ << LOG_SYNC;
	const auto transactions = args["transactional-updates"].as<bool>();
	const auto batched = args["single-transaction"].as<bool>();
	exec.select_db(db);

	if(batched) {
		exec.start_transaction();
	}

	for(const auto& path : migration_paths) {
		try {
			LOG_INFO_GLOB << "Applying " << path << LOG_ASYNC;
			const auto& queries = load_queries(path);

			if(transactions && !batched) {
				exec.start_transaction();
			}

			for(const auto& query : queries) {
				exec.execute(query);
			}

			const std::string filename = std::filesystem::path(path).filename().string();
			std::regex pattern(R"((\w+)_(...+)_(\w+))");
			std::smatch matches;

			if(!std::regex_search(filename, matches, pattern) || matches.size() != 4) {
				throw std::runtime_error("Could not regex parse migration filename");
			}

			exec.insert_migration_meta({
				.core_version = matches[2],
				.installed_by = boost::asio::ip::host_name(),
				.commit_hash = matches[3],
				.file = filename
			});

			if(transactions && !batched) {
				exec.end_transaction();
			}
		} catch(std::exception& e) {
			LOG_ERROR_GLOB << path << ": " << e.what() << LOG_SYNC;

			if (transactions || batched) {
				LOG_ERROR_GLOB << "Migration failed, attempting rollback..." << LOG_SYNC;
				exec.rollback();
			} else {
				LOG_ERROR_GLOB << "Migration failed, you may need to restore your database." << LOG_SYNC;
			}

			return false;
		}
	}

	if(batched) {
		exec.end_transaction();
	}

	return true;
}

bool db_update(const po::variables_map& args, const std::string& db) {
	LOG_TRACE_GLOB << __func__ << LOG_SYNC;
	LOG_INFO_GLOB << "Applying updates for " << db << "..." << LOG_SYNC;

	const auto& details = db_details(args, db);			
	auto executor = db_executor(args["database-type"].as<std::string>(), details);
	const auto db_arg = std::string(db) + ".db-name";
	const auto& db_name = args[db_arg].as<std::string>();

	if(!executor) {
		throw std::runtime_error("Unable to obtain a database executor. Invalid database type?");
	}

	executor->select_db(db_name);
	
	if(!executor->test_connection()) {
		throw std::runtime_error("Unable to establish database connection");
	}

	const auto& db_type = args["database-type"].as<std::string>();
	const std::filesystem::path migrations_dir(args["sql-dir"].as<std::string>() + db_type + "/" + db + "/migrations");

	// Fetch details of all applied migrations on this database
	const auto& applied_migrations = executor->migrations();
	std::vector<std::filesystem::path> paths;

	for(const auto& it : std::filesystem::directory_iterator(migrations_dir)) {
		if(it.path().extension() != ".sql") {
			continue;
		}

		paths.emplace_back(it.path());
	}

	std::sort(paths.begin(), paths.end());
	std::vector<std::string> migration_paths;

	for(const auto& path : paths) {
		// filter out any migrations older than the last applied migration
		if(!applied_migrations.empty() && path.filename() <= applied_migrations.back().file) {
			LOG_DEBUG_GLOB << "Skipping " << path.string() << LOG_SYNC;
			continue;
		}

		migration_paths.emplace_back(path.string());
	}

	LOG_INFO_GLOB << "Database has " << applied_migrations.size() << " migration(s) applied" << LOG_SYNC;
	LOG_INFO_GLOB << "Found " << migration_paths.size() << " applicable migration(s)" << LOG_SYNC;

	if(!applied_migrations.empty()) {
		const auto& last = applied_migrations.back();
		LOG_INFO_GLOB << "Current migration: " << last.file << LOG_SYNC;
	}

	if(migration_paths.empty() && applied_migrations.empty()) {
		LOG_WARN_GLOB << "The database has no migration history and no applicable migrations were found. "
		                 "No updates applied!" << LOG_SYNC;
		return true;
	} else if(migration_paths.empty()) {
		LOG_INFO_GLOB << "Database appears to already be up to date!" << LOG_ASYNC;
		return true;
	}

	const auto res = apply_updates(args, *executor, migration_paths, db_name);

	if(res) {
		LOG_INFO_GLOB << "Database migrations applied successfully." << LOG_SYNC;
	} else {
		LOG_WARN_GLOB << "Some migrations could not be applied." << LOG_SYNC;
	}

	return res;
}

po::variables_map parse_arguments(int argc, const char* argv[]) {
	po::options_description opt("Options");
	opt.add_options()
		("help,h", "Displays a list of available options")
		("install", po::value<std::vector<std::string>>()->multitoken(),
			"Perform initial installation of the listed database types")
		("update", po::value<std::vector<std::string>>()->multitoken(),
			"Apply any updates to the provided database types. Valid types are: "
			" world, character, login")
		("world.root-user", po::value<std::string>()->default_value("root"),
			"A root database user, or at least one with liberal permissions.")
		("world.root-password", po::value<std::string>()->default_value(""),
			"The password for the provided root user.")
		("login.root-user", po::value<std::string>()->default_value("root"),
			"A root database user, or at least one with liberal permissions.")
		("login.root-password", po::value<std::string>()->default_value(""),
			"The password for the provided root user.")
		("login.set-user", po::value<std::string>(),
			"The login user to create when initial setting up the databases.")
		("login.set-password", po::value<std::string>(),
			"The login password to create when initial setting up the databases.")
		("world.set-user", po::value<std::string>(),
			"The world user to create when initial setting up the databases.")
		("world.set-password", po::value<std::string>(),
			"The world password to create when initial setting up the databases.")
		("login.db-name", po::value<std::string>()->default_value("ember_login"),
			"The login database name used when creating/updating the databases.")
		("login.hostname", po::value<std::string>()->default_value("localhost"),
			"The hostname used when connecting to the login database.")
		("login.port", po::value<std::uint16_t>()->default_value(3306),
			"The port used when connecting to the login database.")
		("world.db-name", po::value<std::string>()->default_value("ember_world"),
			"The world database name used when updating the databases.")
		("world.hostname", po::value<std::string>()->default_value("localhost"),
			"The hostname used when connecting to the world database.")
		("world.port", po::value<std::uint16_t>()->default_value(3306),
			"The port used when connecting to the world database.")
		("sql-dir", po::value<std::string>()->default_value("sql/"),
			"The directory containing the SQL scripts.")
		("database-type", po::value<std::string>()->default_value("mysql"),
			"The database type to connect to (e.g. MySQL).")
		("clean", po::bool_switch()->default_value(false),
			"Drops any existing users or databases if there's a clash during --install. "
			"Useful if you want to restore the database to a clean state or recover from a failed install.")
		("single-transaction", po::bool_switch()->default_value(false),
			"Whether to apply all updates within a single transaction. "
			"Note that not all migrations can be applied transactionally (e.g. DDL queries).")
		("transactional-updates", po::bool_switch()->default_value(false),
			"Whether to use transactions to allow for rolling back updates in the event of failure. "
			"Note that not all migrations can be applied transactionally (e.g. DDL queries).")
		("shutup", po::bool_switch()->default_value(false),
			"Silence the timed warnings displayed during a updates or a --clean install.")
		("verbosity,v", po::value<std::string>()->default_value("trace"),
			"Logging verbosity")
		("fverbosity", po::value<std::string>()->default_value("disabled"),
			"File logging verbosity");

	po::variables_map options;
	
	po::store(po::command_line_parser(argc, argv).options(opt)
			  .style(po::command_line_style::default_style & ~po::command_line_style::allow_guessing)
			  .run(), options);

	if(options.count("help")) {
		std::cout << opt << "\n";
		std::exit(0);
	}

	po::notify(options);
	return options;
}