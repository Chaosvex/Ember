/*
 * Copyright (c) 2021 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Runner.h"
#include <shared/Banner.h>
#include <shared/Version.h>
#include <shared/threading/Utility.h>
#include <shared/util/LogConfig.h>
#include <shared/util/Utility.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <fstream>
#include <iostream>
#include <span>
#include <thread>

using namespace ember;
namespace po = boost::program_options;

int run(std::span<const char*> cmd_args);
po::variables_map parse_arguments(std::span<const char*> cmd_args);

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
	print_banner(dns::APP_NAME);
	util::set_window_title(dns::APP_NAME);

	std::span<const char*> args(argv, argc);
	return run(args);
} catch(std::exception& e) {
	std::cerr << e.what();
	return EXIT_FAILURE;
}

int run(std::span<const char*> cmd_args) {
	const po::variables_map args = parse_arguments(cmd_args);

	log::Logger logger;
	util::configure_logger(logger, args);
	log::global_logger(logger);
	LOG_INFO(logger) << "Logger configured successfully" << LOG_SYNC;

	// Install signal handler
	boost::asio::io_context service;
	boost::asio::signal_set signals(service, SIGINT, SIGTERM);

	signals.async_wait([&](auto error, auto signal) {
		LOG_DEBUG_SYNC(logger, "Received signal {}({})", util::sig_str(signal), signal);
		dns::stop();
		service.stop();
	});

	std::jthread worker([&]() {
		service.run();
	});

	const auto ret = dns::run(args, logger);
	LOG_INFO(logger) << dns::APP_NAME << " terminated" << LOG_SYNC;
	return ret;
}

po::variables_map parse_arguments(std::span<const char*> args) {
	// Command-line options
	po::options_description cmdline_opts("Generic options");
	cmdline_opts.add_options()
		("help", "Displays a list of available options")
		("config,c", po::value<std::string>()->default_value("mdns.conf"),
			 "Path to the configuration file");

	po::positional_options_description pos;
	pos.add("config", 1);

	// Config file options
	po::options_description config_opts("Multicast DNS configuration options");
	config_opts.add(dns::options());

	po::variables_map options;
	po::store(po::command_line_parser(args.size(), args.data()).positional(pos).options(cmdline_opts).run(), options);
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