/*
 * Copyright (c) 2021 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Server.h"
#include "MulticastSocket.h"
#include "NSDService.h"
#include "RequestHandler.h"
#include <logger/Logger.h>
#include <shared/Banner.h>
#include <shared/Version.h>
#include <shared/util/cstring_view.hpp>
#include <shared/util/LogConfig.h>
#include <shared/util/Utility.h>
#include <shared/threading/Utility.h>
#include <spark/Server.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <semaphore>
#include <string>
#include <utility>
#include <cstddef>
#include <cstdlib>

constexpr ember::cstring_view APP_NAME { "MDNS-SD" };

namespace po = boost::program_options;

using namespace ember;
using namespace std::chrono_literals;

void launch(const po::variables_map& args, boost::asio::io_context& service,
            std::binary_semaphore& sem, log::Logger* logger);
int asio_launch(const po::variables_map& args, log::Logger* logger);
po::variables_map parse_arguments(int argc, const char* argv[]);

std::exception_ptr eptr = nullptr;

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
	print_banner(APP_NAME);
	util::set_window_title(APP_NAME);

	const po::variables_map args = parse_arguments(argc, argv);

	log::Logger logger;
	util::configure_logger(logger, args);
	log::global_logger(logger);
	LOG_INFO(logger) << "Logger configured successfully" << LOG_SYNC;

	const auto ret = asio_launch(args, &logger);
	LOG_INFO(logger) << APP_NAME << " terminated" << LOG_SYNC;
	return ret;
} catch(std::exception& e) {
	std::cerr << e.what();
	return EXIT_FAILURE;
}

/*
 * Starts ASIO worker threads, blocking until the launch thread exits
 * upon error or signal handling.
 * 
 * io_context is only stopped after the thread joins to ensure that all
 * services can cleanly shut down upon destruction without requiring
 * explicit shutdown() calls in a signal handler.
 */
int asio_launch(const po::variables_map& args, log::Logger* logger) try {
	boost::asio::io_context service(BOOST_ASIO_CONCURRENCY_HINT_UNSAFE_IO);
	std::binary_semaphore flag(0);

	std::thread thread([&]() {
		thread::set_name("Launcher");
		launch(args, service, flag, logger);
	});

	// Install signal handler
	boost::asio::signal_set signals(service, SIGINT, SIGTERM);

	signals.async_wait([&](auto error, auto signal) {
		LOG_DEBUG_SYNC(logger, "Received signal {}({})", util::sig_str(signal), signal);
		flag.release();
	});

	std::jthread worker(static_cast<std::size_t(boost::asio::io_context::*)()>
		(&boost::asio::io_context::run), &service);
	thread::set_name(worker, "ASIO Worker");

	thread.join();
	service.stop();

	if(eptr) {
		std::rethrow_exception(eptr);
	}

	return EXIT_SUCCESS;
} catch(const std::exception& e) {
	LOG_FATAL(logger) << e.what() << LOG_SYNC;
	return EXIT_FAILURE;
}

void launch(const po::variables_map& args, boost::asio::io_context& service,
            std::binary_semaphore& sem, log::Logger* logger) try {
#ifdef DEBUG_NO_THREADS
	LOG_WARN(logger) << "Compiled with DEBUG_NO_THREADS!" << LOG_SYNC;
#endif

	const auto& iface = args["mdns.interface"].as<std::string>();
	const auto& group = args["mdns.group"].as<std::string>();
	const auto port = args["mdns.port"].as<std::uint16_t>();

	// start multicast DNS services
	auto socket = std::make_unique<dns::MulticastSocket>(service, iface, group, port);
	dns::Server server(std::move(socket), logger);

	const auto& spark_iface = args["spark.address"].as<std::string>();
	const auto spark_port = args["spark.port"].as<std::uint16_t>();

	// start RPC services
	spark::Server spark(service, APP_NAME, spark_iface, spark_port, logger);
	NSDService nsd(spark, *logger);

	dns::RequestHandler handler(logger);

	// All done setting up
	service.dispatch([logger]() {
		LOG_INFO_SYNC(logger, "{} started successfully", APP_NAME);
	});

	sem.acquire();
	LOG_INFO_SYNC(logger, "{} shutting down...", APP_NAME);
} catch(...) {
	eptr = std::current_exception();
}

po::variables_map parse_arguments(int argc, const char* argv[]) {
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
	config_opts.add_options()
		("mdns.interface", po::value<std::string>()->required())
		("mdns.group", po::value<std::string>()->required())
		("mdns.port", po::value<std::uint16_t>()->default_value(5353))
		("spark.address", po::value<std::string>()->required())
		("spark.port", po::value<std::uint16_t>()->required())
		("console_log.verbosity", po::value<std::string>()->required())
		("console_log.filter-mask", po::value<std::uint32_t>()->default_value(0))
		("console_log.colours", po::value<bool>()->required())
		("remote_log.verbosity", po::value<std::string>()->required())
		("remote_log.filter-mask", po::value<std::uint32_t>()->default_value(0))
		("remote_log.service_name", po::value<std::string>()->required())
		("remote_log.host", po::value<std::string>()->required())
		("remote_log.port", po::value<std::uint16_t>()->required())
		("file_log.verbosity", po::value<std::string>()->required())
		("file_log.filter-mask", po::value<std::uint32_t>()->default_value(0))
		("file_log.path", po::value<std::string>()->default_value("world.log"))
		("file_log.timestamp_format", po::value<std::string>())
		("file_log.mode", po::value<std::string>()->required())
		("file_log.size_rotate", po::value<std::uint32_t>()->required())
		("file_log.midnight_rotate", po::value<bool>()->required())
		("file_log.log_timestamp", po::value<bool>()->required())
		("file_log.log_severity", po::value<bool>()->required())
		("metrics.enabled", po::value<bool>()->required())
		("metrics.statsd_host", po::value<std::string>()->required())
		("metrics.statsd_port", po::value<std::uint16_t>()->required());

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