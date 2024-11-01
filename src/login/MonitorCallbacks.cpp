/*
 * Copyright (c) 2015 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "FilterTypes.h"
#include "MonitorCallbacks.h"
#include "NetworkListener.h"
#include <sstream>

namespace ember {

using namespace std::chrono_literals;

void install_net_monitor(Monitor& monitor, const NetworkListener& server, log::Logger& logger) {
	Monitor::Source source{ "network_connections", std::bind(&NetworkListener::connection_count, &server),
		10s, 1000,
		[](std::intmax_t value, std::intmax_t threshold) {
			return value > threshold;
		},
		"High concurrent connection count!",
	};

	monitor.add_source(source, Monitor::Severity::WARN,
		std::bind(monitor_log_callback, std::placeholders::_1, std::placeholders::_2,
		          std::placeholders::_3, std::ref(logger))
	);

}

void monitor_log_callback(const Monitor::Source& source, Monitor::Severity severity,
                          std::intmax_t value, log::Logger& logger) {
	std::stringstream message;
	message << source.key << ":v:" << value << ":t:" << source.threshold << " - ";

	if(source.triggered) {
		message << source.message;
	} else {
		message << "Incident has been resolved.";
	}

	switch(severity) {
		case Monitor::Severity::FATAL:
			LOG_FATAL_FILTER(logger, LF_MONITORING) << message.view() << LOG_ASYNC;
			break;
		case Monitor::Severity::ERROR:
			LOG_ERROR_FILTER(logger, LF_MONITORING) << message.view() << LOG_ASYNC;
			break;
		case Monitor::Severity::WARN:
			LOG_WARN_FILTER(logger, LF_MONITORING) << message.view() << LOG_ASYNC;
			break;
		case Monitor::Severity::INFO:
			LOG_INFO_FILTER(logger, LF_MONITORING) << message.view() << LOG_ASYNC;
			break;
		case Monitor::Severity::DEBUG:
			LOG_DEBUG_FILTER(logger, LF_MONITORING) << message.view() << LOG_ASYNC;
			break;
	}
}

} // ember