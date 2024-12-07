/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Runner.h"
#include "AccountClient.h"
#include "FilterTypes.h"
#include "GameVersion.h"
#include "IntegrityData.h"
#include "LoginHandlerBuilder.h"
#include "MonitorCallbacks.h"
#include "NetworkListener.h"
#include "Patcher.h"
#include "RealmClient.h"
#include "RealmList.h"
#include "SessionBuilders.h"
#include "Survey.h"
#include <logger/Logger.h>
#include <conpool/ConnectionPool.h>
#include <conpool/Policies.h>
#include <conpool/drivers/AutoSelect.h>
#include <shared/metrics/MetricsImpl.h>
#include <shared/metrics/Monitor.h>
#include <shared/metrics/MetricsPoll.h>
#include <shared/threading/ThreadPool.h>
#include <shared/threading/Utility.h>
#include <shared/database/daos/IPBanDAO.h>
#include <shared/database/daos/PatchDAO.h>
#include <shared/database/daos/RealmDAO.h>
#include <shared/database/daos/UserDAO.h>
#include <shared/IPBanCache.h>
#include <shared/util/cstring_view.hpp>
#include <shared/util/Utility.h>
#include <shared/util/xoroshiro128plus.h>
#include <shared/util/STUN.h>
#include <shared/util/PortForward.h>
#include <spark/Server.h>
#include <stun/Client.h>
#include <stun/Utility.h>
#include <botan/version.h>
#include <boost/asio/io_context.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/version.hpp>
#include <boost/program_options.hpp>
#include <pcre.h>
#include <zlib.h>
#include <exception>
#include <functional>
#include <memory>
#include <ranges>
#include <semaphore>
#include <string>
#include <span>
#include <string_view>
#include <stdexcept>
#include <utility>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

using namespace std::chrono_literals;
namespace po = boost::program_options;
namespace ep = ember::connection_pool;

#if defined TARGET_WORKER_COUNT
constexpr std::size_t WORKER_NUM_HINT = TARGET_WORKER_COUNT;
#else
constexpr std::size_t WORKER_NUM_HINT = 16;
#endif

namespace ember::login {

std::exception_ptr eptr = nullptr;
std::binary_semaphore stop_flag { 0 };

void launch(const po::variables_map& args,
            boost::asio::io_context& service,
            std::binary_semaphore& sem,
            log::Logger& logger);

void pool_log_callback(ep::Severity, std::string_view message, log::Logger& logger);
unsigned int check_concurrency(log::Logger& logger);
void print_lib_versions(log::Logger& logger);
std::vector<GameVersion> client_versions();

/*
 * Starts ASIO worker threads, blocking until the launch thread exits
 * upon error or signal handling.
 * 
 * io_context is only stopped after the thread joins to ensure that all
 * services can cleanly shut down upon destruction without requiring
 * explicit shutdown() calls in a signal handler.
 */
int run(const po::variables_map& args, log::Logger& logger) try {
	const auto concurrency = check_concurrency(logger);
	boost::asio::io_context service(concurrency);
	boost::asio::io_context::work work(service);

	std::thread thread([&]() {
		thread::set_name("Launcher");
		launch(args, service, stop_flag, logger);
	});

	// Spawn worker threads for ASIO
	boost::container::small_vector<std::jthread, WORKER_NUM_HINT> workers;

	for(unsigned int i = 0; i < concurrency; ++i) {
		workers.emplace_back(static_cast<std::size_t(boost::asio::io_context::*)()>
							 (&boost::asio::io_context::run), &service);
		thread::set_name(workers[i], "ASIO Worker");
	}


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

	print_lib_versions(logger);

	auto stun = create_stun_client(args);
	const auto stun_enabled = args["stun.enabled"].as<bool>();

	std::future<stun::MappedResult> stun_res;

	if(stun_enabled) {
		stun.log_callback([&logger](const stun::Verbosity verbosity, const stun::Error reason) {
			stun_log_callback(verbosity, reason, logger);
		});

		LOG_INFO(logger) << "Starting STUN query..." << LOG_SYNC;
		stun_res = stun.external_address();
	}

	LOG_INFO(logger) << "Seeding xorshift RNG..." << LOG_SYNC;
	Botan::AutoSeeded_RNG rng;
	auto seed_bytes = std::as_writable_bytes(std::span(rng::xorshift::seed));
	rng.randomize(reinterpret_cast<std::uint8_t*>(seed_bytes.data()), seed_bytes.size_bytes());

	LOG_INFO(logger) << "Initialising database driver..."<< LOG_SYNC;
	const auto& db_config_path = args["database.config_path"].as<std::string>();
	auto driver(drivers::init_db_driver(db_config_path, "login"));
	auto min_conns = args["database.min_connections"].as<unsigned short>();
	auto max_conns = args["database.max_connections"].as<unsigned short>();

	unsigned int concurrency = check_concurrency(logger);

	if(!max_conns) {
		max_conns = concurrency;
	} else if(max_conns != concurrency) {
		LOG_WARN_SYNC(logger, "Max. database connection count may be non-optimal "
		                      "(use {} to match logical core count)", concurrency);
	}

	LOG_INFO(logger) << "Initialising database connection pool..." << LOG_SYNC;
	ep::Pool<decltype(driver), ep::CheckinClean, ep::ExponentialGrowth> pool(
		driver, min_conns, max_conns, 30s
	);

	pool.logging_callback([&](auto severity, auto message) {
		pool_log_callback(severity, message, logger);
	});

	LOG_INFO(logger) << "Initialising DAOs..." << LOG_SYNC; 
	auto user_dao = dal::user_dao(pool);
	auto realm_dao = dal::realm_dao(pool);
	auto patch_dao = dal::patch_dao(pool);
	auto ip_ban_dao = dal::ip_ban_dao(pool); 
	auto ip_ban_cache = IPBanCache(ip_ban_dao.all_bans());

	// Load integrity, patch and survey data
	LOG_INFO(logger) << "Loading client integrity validation data..." << LOG_SYNC;
	IntegrityData bin_data;

	const auto allowed_clients = client_versions(); // move

	if(args["integrity.enabled"].as<bool>()) {
		const auto& bin_path = args["integrity.bin_path"].as<std::string>();
		bin_data.add_versions(allowed_clients, bin_path);
	}

	LOG_INFO(logger) << "Loading patch data..." << LOG_SYNC;

	auto patches = Patcher::load_patches(
		args["patches.bin_path"].as<std::string>(), patch_dao
	);

	Patcher patcher(allowed_clients, patches);
	Survey survey(args["survey.id"].as<std::uint32_t>());

	if(survey.id()) {
		LOG_INFO(logger) << "Loading survey data..." << LOG_SYNC;

		survey.add_data(grunt::Platform::x86, grunt::System::Win,
		                args["survey.path"].as<std::string>());
	}

	LOG_INFO(logger) << "Loading realm list..." << LOG_SYNC;
	RealmList realm_list(realm_dao.get_realms());

	LOG_INFO_SYNC(logger, "Added {} realm(s)", realm_list.realms()->size());

	for(const auto& realm : *realm_list.realms() | std::views::values) {
		LOG_DEBUG_SYNC(logger, "#{} {}", realm.id, realm.name);
	}

	const auto& s_address = args["spark.address"].as<std::string>();
	auto s_port = args["spark.port"].as<std::uint16_t>();
	auto spark_filter = log::Filter(FilterType::LF_SPARK);

	LOG_INFO(logger) << "Starting RPC services..." << LOG_SYNC;
	spark::Server spark(service, "login", s_address, s_port, logger);
	AccountClient acct_svc(spark, logger);
	RealmClient realm_svcv2(spark, realm_list, logger);

	// Start metrics service
	auto metrics = std::make_unique<Metrics>();

	if(args["metrics.enabled"].as<bool>()) {
		LOG_INFO(logger) << "Starting metrics service..." << LOG_SYNC;
		metrics = std::make_unique<MetricsImpl>(
			service, args["metrics.statsd_host"].as<std::string>(),
			args["metrics.statsd_port"].as<std::uint16_t>()
		);
	}

	LOG_INFO_SYNC(logger, "Starting thread pool with {} threads...", concurrency);
	ThreadPool thread_pool(concurrency);

	LoginHandlerBuilder builder(logger, patcher, survey, bin_data, user_dao,
	                            acct_svc, realm_list, *metrics,
	                            args["locale.enforce"].as<bool>(),
	                            args["integrity.enabled"].as<bool>());
	LoginSessionBuilder s_builder(builder, thread_pool);

	const auto& interface = args["network.interface"].as<std::string>();
	const auto port = args["network.port"].as<std::uint16_t>();
	const auto tcp_no_delay = args["network.tcp_no_delay"].as<bool>();

	LOG_INFO_SYNC(logger, "Starting network service...");

	NetworkListener server(
		service, interface, port, tcp_no_delay, s_builder, ip_ban_cache, logger, *metrics
	);

	LOG_INFO_SYNC(logger, "Started network service on {}:{}", interface, server.port());

	// Start monitoring service
	std::unique_ptr<Monitor> monitor;

	if(args["monitor.enabled"].as<bool>()) {
		LOG_INFO(logger) << "Starting monitoring service..." << LOG_SYNC;

		monitor = std::make_unique<Monitor>(	
			service, args["monitor.interface"].as<std::string>(),
			args["monitor.port"].as<std::uint16_t>()
		);

		install_net_monitor(*monitor, server, logger);
		install_pool_monitor(*monitor, pool, logger);
	}

	// Start metrics polling
	MetricsPoll poller(service, *metrics);

	poller.add_source([&pool](Metrics& metrics) {
		metrics.gauge("db_connections", pool.size());
	}, 5s);

	poller.add_source([&server](Metrics& metrics) {
		metrics.gauge("sessions", server.connection_count());
	}, 5s);

	// Misc. information
	LOG_INFO_SYNC(logger, "Max allowed sockets: {}", util::max_sockets_desc());
	std::string builds;

	for(const auto& client : allowed_clients) {
		builds += std::to_string(client.build) + " ";
	}

	LOG_INFO_SYNC(logger, "Allowed client builds: {}", builds);
	
	// Retrieve STUN result and start port forwarding if enabled and STUN succeeded
	std::unique_ptr<util::PortForward> forward;

	if(stun_enabled) {
		const auto result = stun_res.get();
		log_stun_result(stun, result, port, logger);

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

	// All done setting up
	service.dispatch([&]() {
		LOG_INFO_SYNC(logger, "{} started successfully", APP_NAME);
	});
	
	sem.acquire();
	LOG_INFO_SYNC(logger, "{} shutting down...", APP_NAME);
} catch(...) {
	eptr = std::current_exception();
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

po::options_description options() {
	po::options_description opts;
	opts.add_options()
		("locale.enforce", po::value<bool>()->required())
		("patches.bin_path", po::value<std::string>()->required())
		("survey.path", po::value<std::string>()->required())
		("survey.id", po::value<std::uint32_t>()->required())
		("integrity.enabled", po::value<bool>()->default_value(false))
		("integrity.bin_path", po::value<std::string>()->required())
		("spark.address", po::value<std::string>()->required())
		("spark.port", po::value<std::uint16_t>()->required())
		("nsd.host", po::value<std::string>()->required())
		("nsd.port", po::value<std::uint16_t>()->required())
		("stun.enabled", po::value<bool>()->required())
		("stun.server", po::value<std::string>()->required())
		("stun.port", po::value<std::uint16_t>()->required())
		("stun.protocol", po::value<std::string>()->required())
		("forward.enabled", po::value<bool>()->required())
		("forward.method", po::value<std::string>()->required())
		("forward.gateway", po::value<std::string>()->required())
		("network.interface", po::value<std::string>()->required())
		("network.port", po::value<std::uint16_t>()->required())
		("network.tcp_no_delay", po::value<bool>()->default_value(true))
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
		("file_log.path", po::value<std::string>()->default_value("login.log"))
		("file_log.timestamp_format", po::value<std::string>())
		("file_log.mode", po::value<std::string>()->required())
		("file_log.size_rotate", po::value<std::uint32_t>()->required())
		("file_log.midnight_rotate", po::bool_switch()->required())
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

/*
 * This vector defines the client builds that are allowed to connect to the
 * server. All builds in this list should be using the same protocol version.
 */
std::vector<GameVersion> client_versions() {
	return {{1, 12, 1, 5875}, {1, 12, 2, 6005}};
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

} // login, ember