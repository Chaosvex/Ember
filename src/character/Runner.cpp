/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Runner.h"
#include "FilterTypes.h"
#include "CharacterHandler.h"
#include "CharacterService.h"
#include <dbcreader/Reader.h>
#include <conpool/ConnectionPool.h>
#include <conpool/Policies.h>
#include <conpool/drivers/AutoSelect.h>
#include <shared/database/daos/CharacterDAO.h>
#include <shared/threading/ThreadPool.h>
#include <shared/utility/PCREHelper.h>
#include <spark/Server.h>
#include <boost/asio/io_context.hpp>
#include <chrono>
#include <exception>
#include <ranges>
#include <semaphore>
#include <cstddef>
#include <cstdlib>
#include <cstdint>

namespace po = boost::program_options;
namespace ep = ember::connection_pool;
using namespace std::chrono_literals;

namespace ember::character {

std::exception_ptr eptr = nullptr;
std::binary_semaphore stop_flag { 0 };

void launch(const po::variables_map& args,
            boost::asio::io_context& service,
            std::binary_semaphore& sem,
            log::Logger& logger);

void pool_log_callback(ep::Severity, std::string_view message, log::Logger& logger);
unsigned int check_concurrency(log::Logger& logger);

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

void stop() {
	stop_flag.release();
}

void launch(const po::variables_map& args, boost::asio::io_context& service,
            std::binary_semaphore& sem, log::Logger& logger) try {
	#ifdef DEBUG_NO_THREADS
	LOG_WARN(logger) << "Compiled with DEBUG_NO_THREADS!" << LOG_SYNC;
#endif
	LOG_INFO(logger) << "Loading DBC data..." << LOG_SYNC;
	dbc::DiskLoader loader(args["dbc.path"].as<std::string>(), [&](auto message) {
		LOG_DEBUG(logger) << message << LOG_SYNC;
	});

	auto dbc_store = loader.load(
		"ChrClasses", "ChrRaces", "CharBaseInfo", "NamesProfanity", "NamesReserved", "CharSections",
		"CharacterFacialHairStyles", "CharStartBase", "CharStartSpells", "CharStartSkills",
		"CharStartZones", "CharStartOutfit", "AreaTable", "FactionTemplate", "FactionGroup",
		"SpamMessages", "CharStartOutfit", "StartItemQuantities"
	);

	LOG_INFO(logger) << "Resolving DBC references..." << LOG_SYNC;
	dbc::link(dbc_store);

	LOG_INFO(logger) << "Compiling DBC regular expressions..." << LOG_ASYNC;
	std::vector<util::pcre::Result> profanity, reserved, spam;

	for(auto& record : dbc_store.names_profanity | std::views::values) {
		profanity.emplace_back(util::pcre::utf8_jit_compile(record.name));
	}

	for(auto& record : dbc_store.names_reserved | std::views::values) {
		reserved.emplace_back(util::pcre::utf8_jit_compile(record.name));
	}

	for(auto& record : dbc_store.spam_messages | std::views::values) {
		spam.emplace_back(util::pcre::utf8_jit_compile(record.text));
	}

	LOG_INFO(logger) << "Initialising database driver..." << LOG_SYNC;
	const auto&  db_config_path = args["database.config_path"].as<std::string>();
	auto driver(drivers::init_db_driver(db_config_path, "login"));

	LOG_INFO(logger) << "Initialising database connection pool..." << LOG_SYNC;
	auto min_conns = args["database.min_connections"].as<unsigned short>();
	auto max_conns = args["database.max_connections"].as<unsigned short>();
	auto concurrency = check_concurrency(logger);

	if(!max_conns) {
		max_conns = concurrency;
	} else if(max_conns != concurrency) {
		LOG_WARN_SYNC(logger, "Max. database connection count may be non-optimal "
		                      "(use {} to match logical core count)", concurrency);
	}

	LOG_INFO(logger) << "Initialising database connection pool..." << LOG_SYNC;
	ep::Pool<decltype(driver), ep::CheckinClean, ep::ExponentialGrowth> pool(driver, min_conns, max_conns, 30s);
	
	pool.logging_callback([&](auto severity, auto message) {
		pool_log_callback(severity, message, logger);
	});

	LOG_INFO(logger) << "Initialising DAOs..." << LOG_SYNC;
	auto character_dao = dal::character_dao(pool);

	std::locale temp;

	ThreadPool thread_pool(concurrency);
	CharacterHandler handler(std::move(profanity), std::move(reserved), std::move(spam),
	                         dbc_store, character_dao, thread_pool, temp, logger);

	const auto&  s_address = args["spark.address"].as<std::string>();
	auto s_port = args["spark.port"].as<std::uint16_t>();

	LOG_INFO(logger) << "Starting RPC services..." << LOG_SYNC;
	spark::Server spark(service, "character", s_address, s_port, logger);
	CharacterService char_service(spark, handler, logger);
	
	service.dispatch([&]() {
		LOG_INFO_SYNC(logger, "{} started successfully", APP_NAME);
	});

	sem.acquire();
	LOG_INFO_SYNC(logger, "{} shutting down...", APP_NAME);
} catch(...) {
	eptr = std::current_exception();
}

void pool_log_callback(ep::Severity severity, std::string_view message, log::Logger& logger) {
#undef ERROR // Windows moment

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

/*
 * The concurrency level returned is usually the number of logical cores
 * in the machine but the standard doesn't guarantee that it won't be zero.
 * In that case, we just set the minimum concurrency level to one.
 */
unsigned int check_concurrency(log::Logger& logger) {
	unsigned int concurrency = std::thread::hardware_concurrency();

	if(!concurrency) {
		concurrency = 1;
		LOG_WARN(logger) << "Unable to determine concurrency level" << LOG_SYNC;
	}

	return concurrency;
}

po::options_description options() {
	po::options_description opts;
	opts.add_options()
		("dbc.path", po::value<std::string>()->required())
		("spark.address", po::value<std::string>()->required())
		("spark.port", po::value<std::uint16_t>()->required())
		("nsd.host", po::value<std::string>()->required())
		("nsd.port", po::value<std::uint16_t>()->required())
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
		("file_log.path", po::value<std::string>()->default_value("character.log"))
		("file_log.timestamp_format", po::value<std::string>())
		("file_log.mode", po::value<std::string>()->required())
		("file_log.size_rotate", po::value<std::uint32_t>()->required())
		("file_log.midnight_rotate", po::value<bool>()->required())
		("file_log.log_timestamp", po::value<bool>()->required())
		("file_log.log_severity", po::value<bool>()->required())
		("database.config_path", po::value<std::string>()->required())
		("database.min_connections", po::value<unsigned short>()->required())
		("database.max_connections", po::value<unsigned short>()->required())
		("metrics.enabled", po::value<bool>()->required())
		("metrics.statsd_host", po::value<std::string>()->required())
		("metrics.statsd_port", po::value<std::uint16_t>()->required())
		("monitor.enabled", po::value<bool>()->required())
		("monitor.interface", po::value<std::string>()->required())
		("monitor.port", po::value<std::uint16_t>()->required());
	return opts;
}

} // character, ember