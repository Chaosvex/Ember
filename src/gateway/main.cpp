/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Config.h"
#include "Locator.h"
#include "FilterTypes.h"
#include "RealmQueue.h"
#include "AccountClient.h"
#include "EventDispatcher.h"
#include "CharacterClient.h"
#include "RealmService.h"
#include "NetworkListener.h"
#include <conpool/ConnectionPool.h>
#include <conpool/Policies.h>
#include <conpool/drivers/AutoSelect.h>
#include <dbcreader/DBCReader.h>
#include <logger/Logger.h>
#include <nsd/NSD.h>
#include <spark/Server.h>
#include <shared/Banner.h>
#include <shared/util/EnumHelper.h>
#include <shared/Version.h>
#include <shared/util/Utility.h>
#include <shared/util/LogConfig.h>
#include <shared/util/STUN.h>
#include <shared/util/PortForward.h>
#include <shared/database/daos/RealmDAO.h>
#include <shared/database/daos/UserDAO.h>
#include <shared/threading/ServicePool.h>
#include <shared/util/cstring_view.hpp>
#include <shared/util/xoroshiro128plus.h>
#include <stun/Client.h>
#include <stun/Utility.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <boost/version.hpp>
#include <botan/auto_rng.h>
#include <botan/version.h>
#include <pcre.h>
#include <zlib.h>
#include <chrono>
#include <exception>
#include <iostream>
#include <format>
#include <fstream>
#include <memory>
#include <semaphore>
#include <string>
#include <string_view>
#include <stdexcept>
#include <cstdlib>

constexpr ember::cstring_view APP_NAME { "Realm Gateway" };

namespace ep = ember::connection_pool;
namespace po = boost::program_options;

using namespace ember;
using namespace std::chrono_literals;
using namespace std::placeholders;

void launch(const po::variables_map& args, ServicePool& service_pool,
            std::binary_semaphore& sem, log::Logger& logger);
int asio_launch(const po::variables_map& args, log::Logger& logger);
std::optional<Realm> load_realm(const po::variables_map& args, log::Logger& logger);
void print_lib_versions(log::Logger& logger);
unsigned int check_concurrency(log::Logger& logger); // todo, move
po::variables_map parse_arguments(int argc, const char* argv[]);
void pool_log_callback(ep::Severity, std::string_view message, log::Logger& logger);
std::string_view category_name(const Realm& realm, const dbc::DBCMap<dbc::Cfg_Categories>& dbc);

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

	print_lib_versions(logger);
	const auto ret = asio_launch(args, logger);
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
int asio_launch(const po::variables_map& args, log::Logger& logger) try {
	unsigned int concurrency = check_concurrency(logger);

	// Start ASIO service pool
	LOG_INFO_SYNC(logger, "Starting service pool with {} threads", concurrency);
	ServicePool service_pool(concurrency, BOOST_ASIO_CONCURRENCY_HINT_UNSAFE_IO);
	service_pool.run();

	// Install signal handler
	boost::asio::signal_set signals(service_pool.get(), SIGINT, SIGTERM);
	std::binary_semaphore flag(0);

	signals.async_wait([&](auto error, auto signal) {
		LOG_DEBUG_SYNC(logger, "Received signal {}({})", util::sig_str(signal), signal);
		flag.release();
	});

	std::thread thread([&]() {
		thread::set_name("Launcher");
		launch(args, service_pool, flag, logger);
	});

	thread.join();

	if(eptr) {
		std::rethrow_exception(eptr);
	}

	return EXIT_SUCCESS;
} catch(const std::exception& e) {
	LOG_FATAL(logger) << e.what() << LOG_SYNC;
	return EXIT_FAILURE;
}

void launch(const po::variables_map& args, ServicePool& service_pool,
            std::binary_semaphore& sem, log::Logger& logger) try {
#ifdef DEBUG_NO_THREADS
	LOG_WARN(logger) << "Compiled with DEBUG_NO_THREADS!" << LOG_SYNC;
#endif

	auto stun = create_stun_client(args);
	const auto stun_enabled = args["stun.enabled"].as<bool>();

	std::future<stun::MappedResult> stun_res;

	if(stun_enabled) {
		stun.log_callback([&](const stun::Verbosity verbosity, const stun::Error reason) {
			stun_log_callback(verbosity, reason, logger);
		});

		LOG_INFO(logger) << "Starting STUN query..." << LOG_SYNC;
		stun_res = stun.external_address();
	}

	LOG_INFO(logger) << "Seeding xorshift RNG..." << LOG_SYNC;
	Botan::AutoSeeded_RNG rng;
	auto seed_bytes = std::as_writable_bytes(std::span(rng::xorshift::seed));
	rng.randomize(reinterpret_cast<std::uint8_t*>(seed_bytes.data()), seed_bytes.size_bytes());

	LOG_INFO(logger) << "Loading DBC data..." << LOG_SYNC;
	dbc::DiskLoader loader(args["dbc.path"].as<std::string>(), [&](auto message) {
		LOG_DEBUG(logger) << message << LOG_SYNC;
	});

	auto dbc_store = loader.load("AddonData", "Cfg_Categories");

	LOG_INFO(logger) << "Resolving DBC references..." << LOG_SYNC;
	dbc::link(dbc_store);

	auto realm = load_realm(args, logger);
	
	if(!realm) {
		throw std::invalid_argument("Configured realm ID does not exist in database.");
	}
	
	const auto& title = std::format("{} - {}", APP_NAME, realm->name);
	util::set_window_title(title);

	// Validate category & region
	const auto& cat_name = category_name(*realm, dbc_store.cfg_categories);
	LOG_INFO_SYNC(logger, "Serving as gateway for {} ({})", realm->name, cat_name);

	// Set config
	Config config;
	config.max_slots = args["realm.max_slots"].as<unsigned int>();
	config.list_zone_hide = args["quirks.list_zone_hide"].as<bool>();
	config.realm = &*realm;

	// Determine concurrency level
	unsigned int concurrency = check_concurrency(logger);

	if(args.count("misc.concurrency")) {
		concurrency = args["misc.concurrency"].as<unsigned int>();
	}

	LOG_INFO(logger) << "Starting event dispatcher..." << LOG_SYNC;
	EventDispatcher dispatcher(service_pool);

	LOG_INFO(logger) << "Starting Spark service..." << LOG_SYNC;
	const auto& s_address = args["spark.address"].as<std::string>();
	auto s_port = args["spark.port"].as<std::uint16_t>();
	auto spark_filter = log::Filter(FilterType::LF_SPARK);

	auto& service = service_pool.get();
	const auto port = args["network.port"].as<std::uint16_t>();
	const auto& interface = args["network.interface"].as<std::string>();
	const auto tcp_no_delay = args["network.tcp_no_delay"].as<bool>();

	// If the database port differs from the config file port, use the config file port
	if(port != realm->port) {
		LOG_WARN_SYNC(
			logger, "Configured port {} differs from database entry port {}, using {}", port, realm->port, port
		);

		realm->port = port;
		realm->address = std::format("{}:{}", realm->ip, realm->port);
	}

	// Retrieve STUN result and start port forwarding if enabled and STUN succeeded
	std::unique_ptr<util::PortForward> forward;

	if(stun_enabled) {
		const auto result = stun_res.get();
		log_stun_result(stun, result, port, logger);

		if(result) {
			const auto& ip = stun::extract_ip_to_string(*result);
			realm->ip = ip;
			realm->address = std::format("{}:{}", realm->ip, realm->port);
		}

		if(result && args["forward.enabled"].as<bool>()) {
			const auto& mode_str = args["forward.method"].as<std::string>();
			const auto& gateway = args["forward.gateway"].as<std::string>();
			auto mode = util::PortForward::Mode::AUTO;

			if(mode_str == "natpmp") {
				mode = util::PortForward::Mode::PMP_PCP;
			} else if(mode_str == "upnp") {
				mode = util::PortForward::Mode::UPNP;
			} else if(mode_str != "auto") {
				throw std::invalid_argument("Unknown port forwarding method");
			}

			forward = std::make_unique<util::PortForward>(
				logger, service, mode, interface, gateway, port
			);
		}
	}

	LOG_INFO_SYNC(logger, "Realm will be advertised on {}", realm->address);

	RealmQueue queue_service(service_pool.get());
	
	LOG_INFO(logger) << "Starting RPC services..." << LOG_SYNC;
	spark::Server spark(service_pool.get(), "realm", s_address, s_port, logger);
	RealmService realm_svc(spark, *realm, logger);
	AccountClient acct_svc(spark, logger);
	CharacterClient char_svc(spark, config, logger);

	const auto nsd_host = args["nsd.host"].as<std::string>();
	const auto nsd_port = args["nsd.port"].as<std::uint16_t>();

	NetworkServiceDiscovery nds(spark, nsd_host, nsd_port, logger);

	// set services - not the best design pattern but it'll do for now
	Locator::set(&dispatcher);
	Locator::set(&queue_service);
	Locator::set(&realm_svc);
	Locator::set(&acct_svc);
	Locator::set(&char_svc);
	Locator::set(&config);
	
	// Misc. information
	const auto max_socks = util::max_sockets_desc();
	LOG_INFO_SYNC(logger, "Max allowed sockets: {}", max_socks);

	// Start network listener
	LOG_INFO_SYNC(logger, "Starting network service...");

	NetworkListener server(service_pool, interface, port, tcp_no_delay, logger);

	LOG_INFO_SYNC(logger, "Started network service on {}:{}", interface, server.port());

	service.dispatch([&]() {
		realm_svc.set_online();
		LOG_INFO_SYNC(logger, "{} started successfully", APP_NAME);
	});

	sem.acquire();
	LOG_INFO_SYNC(logger, "{} shutting down...", APP_NAME);
} catch(...) {
	eptr = std::current_exception();
}

/*
 * Split from launch() as the DB connection is only needed for
 * loading the initial realm information. If the gateway requires
 * connections elsewhere in the future, this should be merged back.
 */
std::optional<Realm> load_realm(const po::variables_map& args, log::Logger& logger) {
	LOG_INFO(logger) << "Initialising database driver..." << LOG_SYNC;
	const auto& db_config_path = args["database.config_path"].as<std::string>();
	auto driver(drivers::init_db_driver(db_config_path, "login"));

	LOG_INFO(logger) << "Initialising database connection pool..." << LOG_SYNC;
	ep::Pool<decltype(driver), ep::CheckinClean, ep::ExponentialGrowth> pool(driver, 1, 1, 30s);
	
	pool.logging_callback([&](auto severity, auto message) {
		pool_log_callback(severity, message, logger);
	});

	LOG_INFO(logger) << "Initialising DAOs..." << LOG_SYNC;
	auto realm_dao = dal::realm_dao(pool);

	LOG_INFO(logger) << "Retrieving realm information..." << LOG_SYNC;
	return realm_dao.get_realm(args["realm.id"].as<unsigned int>());
}

std::string_view category_name(const Realm& realm, const dbc::DBCMap<dbc::Cfg_Categories>& dbc) {
	for(auto&& [_, record] : dbc) {
		if(record.category == realm.category && record.region == realm.region) {
			return record.name.en_gb;
		}
	}

	throw std::invalid_argument("Unknown category/region combination in database");
}

po::variables_map parse_arguments(int argc, const char* argv[]) {
	//Command-line options
	po::options_description cmdline_opts("Generic options");
	cmdline_opts.add_options()
		("help", "Displays a list of available options")
		("config,c", po::value<std::string>()->default_value("gateway.conf"),
			"Path to the configuration file");

	po::positional_options_description pos; 
	pos.add("config", 1);

	//Config file options
	po::options_description config_opts("Realm gateway configuration options");
	config_opts.add_options()
		("quirks.list_zone_hide", po::value<bool>()->required())
		("dbc.path", po::value<std::string>()->required())
		("misc.concurrency", po::value<unsigned int>())
		("realm.id", po::value<unsigned int>()->required())
		("realm.max_slots", po::value<unsigned int>()->required())
		("realm.reserved_slots", po::value<unsigned int>()->required())
		("spark.address", po::value<std::string>()->required())
		("spark.port", po::value<std::uint16_t>()->required())
		("stun.enabled", po::value<bool>()->required())
		("stun.server", po::value<std::string>()->required())
		("stun.port", po::value<std::uint16_t>()->required())
		("stun.protocol", po::value<std::string>()->required())
		("nsd.host", po::value<std::string>()->required())
		("nsd.port", po::value<std::uint16_t>()->required())
		("forward.enabled", po::value<bool>()->required())
		("forward.method", po::value<std::string>()->required())
		("forward.gateway", po::value<std::string>()->required())
		("network.interface", po::value<std::string>()->required())
		("network.port", po::value<std::uint16_t>()->required())
		("network.tcp_no_delay", po::value<bool>()->required())
		("network.compression", po::value<std::uint8_t>()->required())
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
		("file_log.path", po::value<std::string>()->default_value("gateway.log"))
		("file_log.timestamp_format", po::value<std::string>())
		("file_log.mode", po::value<std::string>()->required())
		("file_log.size_rotate", po::value<std::uint32_t>()->required())
		("file_log.midnight_rotate", po::value<bool>()->required())
		("file_log.log_timestamp", po::value<bool>()->required())
		("file_log.log_severity", po::value<bool>()->required())
		("database.config_path", po::value<std::string>()->required())
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

	const auto& config_path = options["config"].as<std::string>();
	std::ifstream ifs(config_path);

	if(!ifs) {
		throw std::invalid_argument("Unable to open configuration file: " + config_path);
	}

	po::store(po::parse_config_file(ifs, config_opts), options);
	po::notify(options);

	return options;
}

void pool_log_callback(ep::Severity severity, std::string_view message, log::Logger& logger) {
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

void print_lib_versions(log::Logger& logger) {
	LOG_DEBUG(logger)
		<< "Compiled with library versions: " << "\n"
		<< " - Boost " << BOOST_VERSION / 100000 << "."
		<< BOOST_VERSION / 100 % 1000 << "."
		<< BOOST_VERSION % 100 << "\n"
		<< " - " << Botan::version_string() << "\n"
		<< " - " << drivers::DriverType::name()
		<< " ("  << drivers::DriverType::version() << ")" << "\n"
		<< " - PCRE " << PCRE_MAJOR << "." << PCRE_MINOR << "\n"
		<< " - Zlib " << ZLIB_VERSION << LOG_SYNC;
}
