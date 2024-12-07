/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Runner.h"
#include <logger/Logger.h>
#include <shared/Banner.h>
#include <shared/threading/Utility.h>
#include <shared/util/LogConfig.h>
#include <shared/util/Utility.h>
#include <shared/util/cstring_view.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstdlib>

using namespace ember;
namespace po = boost::program_options;

int launch(const po::variables_map& args, log::Logger& logger);
po::variables_map parse_arguments(int argc, const char* argv[]);

/*
 * We want to do the minimum amount of work required to get 
 * logging facilities and crash handlers up and running in main.
 *
 * Exceptions that aren't derived from std::exception are
 * left to the crash handler since we can't get useful information
 * from them.
 */
int main(int argc, const char* argv[]) try {
	thread::set_name("Main");
	print_banner(world::APP_NAME);
	util::set_window_title(world::APP_NAME);

	const po::variables_map args = parse_arguments(argc, argv);

	log::Logger logger;
	util::configure_logger(logger, args);
	log::global_logger(logger);

	LOG_INFO(logger) << "Logger configured successfully" << LOG_SYNC;
	const auto ret = launch(args, logger);
	LOG_INFO_SYNC(logger, "{} terminated ({})", world::APP_NAME, ret);
	return ret;
} catch(const std::exception& e) {
	std::cerr << e.what();
	return EXIT_FAILURE;
}

int launch(const po::variables_map& args, log::Logger& logger) try {
	return world::run(args, logger);
} catch(const std::exception& e) {
	LOG_FATAL_SYNC(logger, "{}", e.what());
	return EXIT_FAILURE;
}

po::variables_map parse_arguments(const int argc, const char* argv[]) {
	// Command-line options
	po::options_description cmdline_opts("Generic options");
	cmdline_opts.add_options()
		("help", "Displays a list of available options")
		("database.config_path,d", po::value<std::string>(),
			"Path to the database configuration file")
		("config,c", po::value<std::string>()->default_value("world.conf"),
			"Path to the configuration file");

	po::positional_options_description pos;
	pos.add("config", 1);

	// Config file options
	po::options_description config_opts("World configuration options");
	config_opts.add(world::options());

	po::variables_map options;
	po::store(po::command_line_parser(argc, argv).positional(pos).options(cmdline_opts).run(), options);
	po::notify(options);

	if(options.count("help")) {
		std::cout << cmdline_opts;
		std::exit(EXIT_SUCCESS);
	}

	const auto& config_path = options["config"].as<std::string>();
	std::ifstream ifs(config_path);

	if(!ifs) {
		throw std::invalid_argument("Unable to open configuration file: " + config_path);
	}

	po::store(po::parse_config_file(ifs, config_opts), options);
	po::notify(options);

	return options;
}