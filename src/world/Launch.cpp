/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Launch.h"
#include "MapRunner.h"
#include "utilities/Utility.h"
#include <dbcreader/Reader.h>
#include <boost/program_options.hpp>
#include <vector>
#include <cstdint>
#include <cstdlib>

namespace po = boost::program_options;

namespace ember::world {

int launch(const boost::program_options::variables_map& args, log::Logger& logger) {
	LOG_INFO(logger) << "Loading DBC data..." << LOG_SYNC;

	dbc::DiskLoader loader(args["dbc.path"].as<std::string>(), [&](auto message) {
		LOG_DEBUG(logger) << message << LOG_SYNC;
	});

	auto dbc_store = loader.load("Map", "GameTips");

	LOG_INFO(logger) << "Resolving DBC references..." << LOG_SYNC;
	dbc::link(dbc_store);

	const auto tip = random_tip(dbc_store.game_tips);

	if(!tip.empty()) {
		LOG_INFO_SYNC(logger, "Tip: {}", tip);
	}

	const auto& maps = args["world.map_id"].as<std::vector<std::int32_t>>();

	if(!validate_maps(maps, dbc_store.map, logger)) {
		return EXIT_FAILURE;
	}

	LOG_INFO_SYNC(logger, "Serving as world server for maps:");
	print_maps(maps, dbc_store.map, logger);

	run(logger);

	return EXIT_SUCCESS;
}

po::options_description options() {
	po::options_description opts;
	opts.add_options()
		("database.min_connections", po::value<unsigned short>()->required())
		("database.max_connections", po::value<unsigned short>()->required())
		("database.config_path", po::value<std::string>()->required())
		("network.interface", po::value<std::string>()->required())
		("network.port", po::value<std::uint16_t>()->required())
		("network.tcp_no_delay", po::value<bool>()->required())
		("dbc.path", po::value<std::string>()->required())
		("spark.address", po::value<std::string>()->required())
		("spark.port", po::value<std::uint16_t>()->required())
		("nsd.host", po::value<std::string>()->required())
		("nsd.port", po::value<std::uint16_t>()->required())
		("world.id", po::value<std::uint32_t>()->required())
		("world.map_id", po::value<std::vector<std::int32_t>>()->required());
	return opts;
}

} // world, ember