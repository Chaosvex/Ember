/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <mdns/Runner.h>
#include <logger/Logger.h>
#include <shared/Banner.h>
#include <shared/util/cstring_view.hpp>
#include <shared/util/LogConfig.h>
#include <shared/threading/Utility.h>
#include <shared/util/Utility.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <span>
#include <thread>
#include <vector>

constexpr ember::cstring_view APP_NAME { "Fusion" };

namespace po = boost::program_options;

using namespace ember;

po::variables_map parse_arguments(int, const char*[]);
po::variables_map load_options(const std::string&, const po::options_description&);
void launch(const po::variables_map&, log::Logger&);
void launch_dns(const po::variables_map&, log::Logger&);
void launch_login(const po::variables_map&, log::Logger&);
void launch_gateway(const po::variables_map&, log::Logger&);
void launch_account(const po::variables_map&, log::Logger&);
void launch_character(const po::variables_map&, log::Logger&);
void launch_world(const po::variables_map&, log::Logger&);
void stop_services();

int main(int argc, const char* argv[]) try {
	thread::set_name("Main");
	print_banner(APP_NAME);
	util::set_window_title(APP_NAME);

	const po::variables_map args = parse_arguments(argc, argv);

	log::Logger logger;
	util::configure_logger(logger, args);
	log::global_logger(logger);

	std::span<const char*> cmd_args(argv, argc);
	launch(args, logger);
	LOG_INFO_SYNC(logger, "{} terminated", APP_NAME);
} catch(std::exception& e) {
	std::cerr << e.what();
	return EXIT_FAILURE;
}

void launch(const po::variables_map& args, log::Logger& logger) {
	// Install signal handler
	boost::asio::io_context service;
	boost::asio::signal_set signals(service, SIGINT, SIGTERM);

	signals.async_wait([&](auto error, auto signal) {
		LOG_DEBUG_SYNC(logger, "Received signal {}({})", util::sig_str(signal), signal);
		stop_services();
		service.stop();
	});

	std::jthread worker([&]() {
		service.run();
	});

	// Start services
	std::vector<std::jthread> services;

	if(args.count("dns.active")) {
		services.emplace_back(std::jthread([&]() {
			launch_dns(args, logger);
		}));
	}

	if(services.empty()) {
		LOG_INFO_SYNC(logger, "No services specified? Nothing to do, farewell.");
	}
}

void stop_services() {
	dns::stop();
}

void launch_dns(const po::variables_map& args, log::Logger& logger) try {
	LOG_INFO_SYNC(logger, "Starting DNS service...");

	const auto& conf_path = args["dns.config"].as<std::string>();
	auto opts = load_options(conf_path, dns::options());

	if(!opts.contains("console_log.prefix")) {
		boost::any prefix = std::string("[mdns]");
		opts.insert({ "console_log.prefix", po::variable_value(prefix, false) });
	}

	log::Logger service_logger;
	util::configure_logger(service_logger, opts);
	const auto res = dns::run(opts, service_logger);

	if(res != EXIT_SUCCESS) {
		LOG_FATAL_SYNC(logger, "DNS service terminated abnormally, aborting");
		std::exit(res);
	}
} catch(std::exception& e) {
	LOG_FATAL_SYNC(logger, "DNS error: {}", e.what());
	std::exit(EXIT_FAILURE);
}

void launch_login(std::span<const char*> cmd_args, log::Logger& logger) try {
	LOG_INFO_SYNC(logger, "Starting login service...");

	// todo
} catch(std::exception& e) {
	LOG_FATAL_SYNC(logger, "Login error: {}", e.what());
	std::exit(EXIT_FAILURE);
}

void launch_gateway(std::span<const char*> cmd_args, log::Logger& logger) try {
	LOG_INFO_SYNC(logger, "Starting gateway service...");

	// todo
} catch(std::exception& e) {
	LOG_FATAL_SYNC(logger, "Gateway error: {}", e.what());
	std::exit(EXIT_FAILURE);
}

void launch_account(std::span<const char*> cmd_args, log::Logger& logger) try {
	LOG_INFO_SYNC(logger, "Starting account service...");

	// todo
} catch(std::exception& e) {
	LOG_FATAL_SYNC(logger, "Account error: {}", e.what());
	std::exit(EXIT_FAILURE);
}

void launch_character(std::span<const char*> cmd_args, log::Logger& logger) try {
	LOG_INFO_SYNC(logger, "Starting character service...");

	// todo
} catch(std::exception& e) {
	LOG_FATAL_SYNC(logger, "Character error: {}", e.what());
	std::exit(EXIT_FAILURE);
}

void launch_world(std::span<const char*> cmd_args, log::Logger& logger) try {
	LOG_INFO_SYNC(logger, "Starting world service...");

	// todo
} catch(std::exception& e) {
	LOG_FATAL_SYNC(logger, "World error: {}", e.what());
	std::exit(EXIT_FAILURE);
}

po::variables_map load_options(const std::string& config_path, const po::options_description& opt_desc) {
	std::ifstream ifs(config_path);

	if(!ifs) {
		throw std::invalid_argument("Unable to open configuration file: " + config_path);
	}

	po::variables_map options;
	po::store(po::parse_config_file(ifs, opt_desc, true), options);
	po::notify(options);

	return options;
}

po::variables_map parse_arguments(int argc, const char* argv[]) {
	// Command-line options
	po::options_description cmdline_opts("Generic options");
	cmdline_opts.add_options()
		("help", "Displays a list of available options")
		("config,c", po::value<std::string>()->default_value("fusion.conf"),
			 "Path to the configuration file");

	po::positional_options_description pos;
	pos.add("config", 1);

	// Config file options
	po::options_description config_opts("Fusion configuration options");
	config_opts.add_options()
		("dns.active", po::value<bool>()->required())
		("dns.config", po::value<std::string>()->required())
		("account.active", po::value<bool>()->required())
		("account.config", po::value<std::string>()->required())
		("character.active", po::value<bool>()->required())
		("character.config", po::value<std::string>()->required())
		("gateway.active", po::value<bool>()->required())
		("gateway.config", po::value<std::string>()->required())
		("world.active", po::value<bool>()->required())
		("world.config", po::value<std::string>()->required())
		("login.active", po::value<bool>()->required())
		("login.config", po::value<std::string>()->required())
		("console_log.verbosity", po::value<std::string>()->required())
		("console_log.filter-mask", po::value<std::uint32_t>()->default_value(0))
		("console_log.colours", po::value<bool>()->required())
		("console_log.prefix", po::value<std::string>()->default_value(""))
		("remote_log.verbosity", po::value<std::string>()->required())
		("remote_log.filter-mask", po::value<std::uint32_t>()->default_value(0))
		("remote_log.service_name", po::value<std::string>()->required())
		("remote_log.host", po::value<std::string>()->required())
		("remote_log.port", po::value<std::uint16_t>()->required())
		("file_log.verbosity", po::value<std::string>()->required())
		("file_log.filter-mask", po::value<std::uint32_t>()->default_value(0))
		("file_log.path", po::value<std::string>()->default_value("fusion.log"))
		("file_log.timestamp_format", po::value<std::string>())
		("file_log.mode", po::value<std::string>()->required())
		("file_log.size_rotate", po::value<std::uint32_t>()->required())
		("file_log.midnight_rotate", po::value<bool>()->required())
		("file_log.log_timestamp", po::value<bool>()->required())
		("file_log.log_severity", po::value<bool>()->required());

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