/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Utility.h"
#include <logger/Logger.h>
#include <boost/container/static_vector.hpp>
#include <algorithm>
#include <random>
#include <string_view>

namespace ember {

std::string random_tip(const dbc::Store<dbc::GameTips>& tips) {
	boost::container::static_vector<dbc::GameTips, 1> out;
	std::mt19937 gen{std::random_device{}()};
	std::ranges::sample(tips.values(), std::back_inserter(out), 1, gen);

	if(out.empty()) {
		return {};
	}

	// trim any leading formatting that we can't make use of
	std::string_view text(out.front().text.en_gb);
	std::string_view needle("|cffffd100Tip:|r ");
	
	if(text.find(needle) != text.npos) {
		text = text.substr(needle.size(), text.size());
	}

	// trim any trailing newlines that we don't want to print
	if(auto pos = text.find_first_of('\n'); pos != text.npos) {
		text = text.substr(0, pos);
	}

	return std::string(text);
}

bool validate_maps(std::span<const std::int32_t> maps, const dbc::Store<dbc::Map>& dbc, log::Logger& logger) {
	const auto validate = [&](const auto id) {
		auto it = std::ranges::find_if(dbc, [&](auto& record) {
			return record.second.id == id;
		});

		if(it == dbc.end()) {
			LOG_ERROR_SYNC(logger, "Unknown map ID ({}) specified", id);
			return false;
		}

		auto& [_, map] = *it;

		if(map.instance_type != dbc::Map::InstanceType::NORMAL) {
			LOG_ERROR_SYNC(logger, "Map {} ({}) is not an open world area", map.id, map.map_name.en_gb);
			return false;
		}

		return true;
	};

	return std::ranges::find(maps, false, validate) == maps.end();
}

void print_maps(std::span<const std::int32_t> maps, const dbc::Store<dbc::Map>& dbc, log::Logger& logger) {
	for(auto id : maps) {
		LOG_INFO_SYNC(logger, " - {}", dbc[id]->map_name.en_gb);
	}
}

} // ember