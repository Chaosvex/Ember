/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Runner.h"
#include "AccountService.h"
#include "AccountHandler.h"
#include "FilterTypes.h"
#include "Sessions.h"
#include <logger/Logger.h>
#include <conpool/ConnectionPool.h>
#include <conpool/Policies.h>
#include <conpool/drivers/AutoSelect.h>
#include <shared/database/daos/UserDAO.h>
#include <shared/metrics/MetricsImpl.h>
#include <shared/metrics/Monitor.h>
#include <shared/threading/ThreadPool.h>
#include <shared/threading/Utility.h>
#include <spark/Server.h>
#include <boost/asio/io_context.hpp>
#include <exception>
#include <semaphore>
#include <string_view>
#include <thread>
#include <cstddef>
#include <cstdlib>
#include <cstdint>

namespace po = boost::program_options;
namespace ep = ember::connection_pool;

namespace ember::account {

std::exception_ptr eptr = nullptr;
std::binary_semaphore stop_flag { 0 };

void launch(const po::variables_map& args,
            boost::asio::io_context& service,
            std::binary_semaphore& sem,
            log::Logger& logger);

void pool_log_callback(ep::Severity, std::string_view message, log::Logger& logger);

/*
 * Starts ASIO worker threads, blocking until the launch thread exits
 * upon error or signal handling.
 * 
 * io_context is only stopped after the thread joins to ensure that all
 * services can cleanly shut down upon destruction without requiring
 * explicit shutdown() calls in a signal handler.
 */
int run(const po::variables_map& args, log::Logger& logger) try {
	boost::asio::io_context service(BOOST_ASIO_CONCURRENCY_HINT_UNSAFE_IO);
	boost::asio::io_context::work work(service);

	std::thread thread([&]() {
		thread::set_name("Launcher");
		launch(args, service, stop_flag, logger);
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
            std::binary_semaphore& sem, log::Logger& logger) try {
	constexpr auto concurrency = 1u; // temp
	LOG_INFO_SYNC(logger, "Starting thread pool with {} threads...", concurrency);
	ThreadPool thread_pool(concurrency);

	LOG_INFO(logger) << "Initialising database driver..."<< LOG_SYNC;
	const auto& db_config_path = args["database.config_path"].as<std::string>();
	auto driver(drivers::init_db_driver(db_config_path, "login"));
	auto min_conns = args["database.min_connections"].as<unsigned short>();
	auto max_conns = args["database.max_connections"].as<unsigned short>();

	LOG_INFO(logger) << "Initialising database connection pool..." << LOG_SYNC;

	ep::Pool<decltype(driver), ep::CheckinClean, ep::ExponentialGrowth> pool(
		driver, min_conns, max_conns, 30s
	);

	pool.logging_callback([&](auto severity, auto message) {
		pool_log_callback(severity, message, logger);
	});

	LOG_INFO(logger) << "Initialising DAOs..." << LOG_SYNC; 
	auto user_dao = dal::user_dao(pool);

	LOG_INFO(logger) << "Initialising account handler..." << LOG_SYNC; 
	AccountHandler handler(user_dao, thread_pool);

	LOG_INFO(logger) << "Starting RPC services..." << LOG_SYNC;
	const auto& s_address = args["spark.address"].as<std::string>();
	auto s_port = args["spark.port"].as<std::uint16_t>();

	Sessions sessions(true);

	spark::Server spark(service, "account", s_address, s_port, logger);
	AccountService acct_service(spark, handler, sessions, logger);

	service.dispatch([&]() {
		LOG_INFO_SYNC(logger, "{} started successfully", APP_NAME);
	});

	sem.acquire();

	LOG_INFO_SYNC(logger, "{} shutting down...", APP_NAME);
} catch(...) {
	eptr = std::current_exception();
}

void stop() {
	stop_flag.release();
}

po::options_description options() {
	po::options_description opts;
	opts.add_options()
		("spark.address,", po::value<std::string>()->required())
		("spark.port", po::value<std::uint16_t>()->required())
		("nsd.host", po::value<std::string>()->required())
		("nsd.port", po::value<std::uint16_t>()->required())
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
	return opts;
}

void pool_log_callback(ep::Severity severity, std::string_view message, log::Logger& logger) {
	switch(severity) {
		case ep::Severity::DEBUG:
			LOG_DEBUG(logger) << message << LOG_ASYNC;
			break;
		case ep::Severity::INFO:
			LOG_INFO(logger) << message << LOG_ASYNC;
			break;
		case ep::Severity::WARN:
			LOG_WARN(logger) << message << LOG_ASYNC;
			break;
		case ep::Severity::ERROR:
			LOG_ERROR(logger) << message << LOG_ASYNC;
			break;
		case ep::Severity::FATAL:
			LOG_FATAL(logger) << message << LOG_ASYNC;
			break;
		default:
			LOG_ERROR(logger) << "Unhandled pool log callback severity" << LOG_ASYNC;
			LOG_ERROR(logger) << message << LOG_ASYNC;
	}	
}

} // account, ember