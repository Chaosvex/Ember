/*
 * Copyright (c) 2016 - 2024 Ember
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
#include <logger/Logger.h>
#include <shared/Banner.h>
#include <shared/Version.h>
#include <shared/database/daos/CharacterDAO.h>
#include <shared/threading/ThreadPool.h>
#include <shared/threading/Utility.h>
#include <shared/util/cstring_view.hpp>
#include <shared/util/LogConfig.h>
#include <shared/util/PCREHelper.h>
#include <shared/util/Utility.h>
#include <spark/Server.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <exception>
#include <iostream>
#include <fstream>
#include <semaphore>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <cstdlib>

namespace ep = ember::connection_pool;
namespace po = boost::program_options;

using namespace ember;
using namespace std::chrono_literals;

void launch(const po::variables_map& args, boost::asio::io_context& service,
            std::binary_semaphore& sem, log::Logger& logger);
int asio_launch(const po::variables_map& args, log::Logger& logger);
unsigned int check_concurrency(log::Logger& logger); // todo, move
po::variables_map parse_arguments(int argc, const char* argv[]);
void pool_log_callback(ep::Severity, std::string_view message, log::Logger& logger);

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
	print_banner(character::APP_NAME);
	util::set_window_title(character::APP_NAME);

	const po::variables_map args = parse_arguments(argc, argv);

	log::Logger logger;
	util::configure_logger(logger, args);
	log::global_logger(logger);
	LOG_INFO(logger) << "Logger configured successfully" << LOG_SYNC;

	const auto ret = asio_launch(args, logger);
	LOG_INFO(logger) << character::APP_NAME << " terminated" << LOG_SYNC;
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
int asio_launch(const po::variables_map& args, log::Logger& logger) try {
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

	LOG_INFO_GLOB << "Compiling DBC regular expressions..." << LOG_ASYNC;
	std::vector<util::pcre::Result> profanity, reserved, spam;

	for(auto& [_, record] : dbc_store.names_profanity) {
		profanity.emplace_back(util::pcre::utf8_jit_compile(record.name));
	}

	for(auto& [_, record] : dbc_store.names_reserved) {
		reserved.emplace_back(util::pcre::utf8_jit_compile(record.name));
	}

	for(auto& [_, record] : dbc_store.spam_messages) {
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
	auto spark_filter = log::Filter(FilterType::LF_SPARK);

	LOG_INFO(logger) << "Starting RPC services..." << LOG_SYNC;
	spark::Server spark(service, "character", s_address, s_port, logger);
	CharacterService char_service(spark, handler, logger);
	
	service.dispatch([&]() {
		LOG_INFO_SYNC(logger, "{} started successfully", character::APP_NAME);
	});

	sem.acquire();
	LOG_INFO_SYNC(logger, "{} shutting down...", character::APP_NAME);
} catch(...) {
	eptr = std::current_exception();
}

po::variables_map parse_arguments(int argc, const char* argv[]) {
	//Command-line options
	po::options_description cmdline_opts("Generic options");
	cmdline_opts.add_options()
		("help", "Displays a list of available options")
		("config,c", po::value<std::string>()->default_value("character.conf"),
			"Path to the configuration file");

	po::positional_options_description pos; 
	pos.add("config", 1);

	//Config file options
	po::options_description config_opts("Character service configuration options");
	config_opts.add_options()
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

	po::variables_map options;
	po::store(po::command_line_parser(argc, argv).positional(pos).options(cmdline_opts).run(), options);
	po::notify(options);

	if(options.count("help")) {
		std::cout << cmdline_opts;
		std::exit(EXIT_SUCCESS);
	}

	const std::string& config_path = options["config"].as<std::string>();
	std::ifstream ifs(config_path);

	if(!ifs) {
		throw std::invalid_argument("Unable to open configuration file: " + config_path);
	}

	po::store(po::parse_config_file(ifs, config_opts), options);
	po::notify(options);

	return options;
}

void pool_log_callback(ep::Severity severity, std::string_view message, log::Logger& logger) {
	#undef ERROR // Windows moment

	switch(severity) {
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