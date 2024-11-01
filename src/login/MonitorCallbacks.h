/*
 * Copyright (c) 2015 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <logger/Logger.h>
#include <shared/metrics/Monitor.h>
#include <functional>
#include <cstdint>

namespace ember {

using namespace std::chrono_literals;

class NetworkListener;

void install_net_monitor(Monitor& monitor, const NetworkListener& server, log::Logger& logger);
void monitor_log_callback(const Monitor::Source& source, Monitor::Severity severity,
                          std::intmax_t value, log::Logger& logger);

template<typename T>
void install_pool_monitor(Monitor& monitor, const T& pool, log::Logger& logger) {
	Monitor::Source source{ "db_pool_size", std::bind(&T::size, &pool),
		30s, 0,
		[](std::intmax_t value, std::intmax_t threshold) {
			return value == threshold;
		},
		"Database connection pool is empty!",
	};

	monitor.add_source(source, Monitor::Severity::ERROR,
		std::bind(monitor_log_callback, std::placeholders::_1, std::placeholders::_2,
		          std::placeholders::_3, std::ref(logger))
	);
}

} // ember