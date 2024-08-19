/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "FilterTypes.h"
//#include "MonitorCallbacks.h"
#include "Service.h"
#include "Sessions.h"
#include <spark/Spark.h>
#include <logger/Logging.h>
#include <conpool/ConnectionPool.h>
#include <conpool/Policies.h>
#include <conpool/drivers/AutoSelect.h>
#include <shared/Banner.h>
#include <shared/util/Utility.h>
#include <shared/util/LogConfig.h>
#include <shared/metrics/MetricsImpl.h>
#include <shared/metrics/Monitor.h>
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>
#include <utility>
#include <cstddef>
#include <cstdint>

constexpr ember::cstring_view APP_NAME { "Account Daemon" };

namespace el = ember::log;
namespace es = ember::spark;
namespace ep = ember::connection_pool;
namespace po = boost::program_options;

using namespace ember;

void launch(const po::variables_map& args, boost::asio::io_context& service,
            std::binary_semaphore& sem, log::Logger* logger);
int asio_launch(const po::variables_map& args, log::Logger* logger);
po::variables_map parse_arguments(int argc, const char* argv[]);
void pool_log_callback(ep::Severity, std::string_view message, el::Logger* logger);

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
	print_banner(APP_NAME);
	util::set_window_title(APP_NAME);
	
	const po::variables_map args = parse_arguments(argc, argv);

	auto logger = util::init_logging(args);
	el::set_global_logger(logger.get());
	LOG_INFO(logger) << "Logger configured successfully" << LOG_SYNC;

	const auto ret = asio_launch(args, logger.get());
	LOG_INFO_SYNC(logger, "{} terminated", APP_NAME);
	return ret;
} catch(const std::exception& e) {
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
		LOG_DEBUG_SYNC(logger, "Received signal {}", signal);
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
	LOG_INFO(logger) << "Starting Spark service..." << LOG_SYNC;
	const auto& s_address = args["spark.address"].as<std::string>();
	auto s_port = args["spark.port"].as<std::uint16_t>();
	const auto& mcast_group = args["spark.multicast_group"].as<std::string>();
	const auto& mcast_iface = args["spark.multicast_interface"].as<std::string>();
	auto mcast_port = args["spark.multicast_port"].as<std::uint16_t>();
	auto spark_filter = el::Filter(FilterType::LF_SPARK);

	es::Service spark("account", service, s_address, s_port, logger);
	es::ServiceDiscovery discovery(service, s_address, s_port, mcast_iface, mcast_group,
	                               mcast_port, logger);

	Sessions sessions(true);
	Service net_service(sessions, spark, discovery, logger);

	service.dispatch([logger]() {
		LOG_INFO_SYNC(logger, "{} started successfully", APP_NAME);
	});

	sem.acquire();

	LOG_INFO_SYNC(logger, "{} shutting down...", APP_NAME);
} catch(...) {
	eptr = std::current_exception();
}

po::variables_map parse_arguments(int argc, const char* argv[]) {
	//Command-line options
	po::options_description cmdline_opts("Generic options");
	cmdline_opts.add_options()
		("help", "Displays a list of available options")
		("database.config_path,d", po::value<std::string>(),
			"Path to the database configuration file")
		("config,c", po::value<std::string>()->default_value("account.conf"),
			"Path to the configuration file");

	po::positional_options_description pos; 
	pos.add("config", 1);

	//Config file options
	po::options_description config_opts("Account configuration options");
	config_opts.add_options()
		("spark.address,", po::value<std::string>()->required())
		("spark.port", po::value<std::uint16_t>()->required())
		("spark.multicast_interface,", po::value<std::string>()->required())
		("spark.multicast_group", po::value<std::string>()->required())
		("spark.multicast_port", po::value<std::uint16_t>()->required())
		("console_log.verbosity", po::value<std::string>()->required())
		("console_log.filter-mask", po::value<std::uint32_t>()->default_value(0))
		("console_log.colours", po::bool_switch()->required())
		("remote_log.verbosity", po::value<std::string>()->required())
		("remote_log.filter-mask", po::value<std::uint32_t>()->default_value(0))
		("remote_log.service_name", po::value<std::string>()->required())
		("remote_log.host", po::value<std::string>()->required())
		("remote_log.port", po::value<std::uint16_t>()->required())
		("file_log.verbosity", po::value<std::string>()->required())
		("file_log.filter-mask", po::value<std::uint32_t>()->default_value(0))
		("file_log.path", po::value<std::string>()->default_value("account.log"))
		("file_log.timestamp_format", po::value<std::string>())
		("file_log.mode", po::value<std::string>()->required())
		("file_log.size_rotate", po::value<std::uint32_t>()->required())
		("file_log.midnight_rotate", po::bool_switch()->required())
		("file_log.log_timestamp", po::bool_switch()->required())
		("file_log.log_severity", po::bool_switch()->required())
		("database.config_path", po::value<std::string>()->required())
		("database.min_connections", po::value<unsigned short>()->required())
		("database.max_connections", po::value<unsigned short>()->required())
		("metrics.enabled", po::bool_switch()->required())
		("metrics.statsd_host", po::value<std::string>()->required())
		("metrics.statsd_port", po::value<std::uint16_t>()->required())
		("monitor.enabled", po::bool_switch()->required())
		("monitor.interface", po::value<std::string>()->required())
		("monitor.port", po::value<std::uint16_t>()->required());

	po::variables_map options;
	po::store(po::command_line_parser(argc, argv).positional(pos).options(cmdline_opts).run(), options);
	po::notify(options);

	if(options.count("help")) {
		std::cout << cmdline_opts << "\n";
		std::exit(0);
	}

	const std::string& config_path = options["config"].as<std::string>();
	std::ifstream ifs(config_path);

	if(!ifs) {
		std::string message("Unable to open configuration file: " + config_path);
		throw std::invalid_argument(message);
	}

	po::store(po::parse_config_file(ifs, config_opts), options);
	po::notify(options);

	return options;
}

void pool_log_callback(ep::Severity severity, std::string_view message, el::Logger* logger) {
	/*switch(severity) {
		case ep::Severity::DEBUG:
			LOG_DEBUG_FILTER(logger, LF_DB_CONN_POOL) << message << LOG_ASYNC;
			break;
		case ep::Severity::INFO:
			LOG_INFO_FILTER(logger, LF_DB_CONN_POOL) << message << LOG_ASYNC;
			break;
		case ep::Severity::WARN:
			LOG_WARN_FILTER(logger, LF_DB_CONN_POOL) << message << LOG_ASYNC;
			break;
		case ep::Severity::ERROR:
			LOG_ERROR_FILTER(logger, LF_DB_CONN_POOL) << message << LOG_ASYNC;
			break;
		case ep::Severity::FATAL:
			LOG_FATAL_FILTER(logger, LF_DB_CONN_POOL) << message << LOG_ASYNC;
			break;
		default:
			LOG_ERROR_FILTER(logger, LF_DB_CONN_POOL) << "Unhandled pool log callback severity" << LOG_ASYNC;
			LOG_ERROR_FILTER(logger, LF_DB_CONN_POOL) << message << LOG_ASYNC;
	}	*/
}