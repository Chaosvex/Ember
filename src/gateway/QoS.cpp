/*
 * Copyright (c) 2016 - 2019 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "QoS.h"
#include "ServerConfig.h"
#include "SessionManager.h"
#include "ConnectionStats.h"

namespace ember::gateway {

void QoS::set_timer() {
	timer_.expires_from_now(TIMER_FREQUENCY);
	timer_.async_wait([this](const boost::system::error_code& ec) {
		if(!ec) { // if ec is set, the timer was aborted (shutdown)
			measure_bandwidth();
		}
	});
}

void QoS::measure_bandwidth() {
	auto stats = sessions_.aggregate_stats();
	auto timer_ms = std::chrono::duration_cast<std::chrono::milliseconds>(TIMER_FREQUENCY);
	auto seconds = timer_ms.count() / 1000.0;

	auto out_per_sec = (stats.bytes_out - last_bandwidth_out_) / seconds;
	auto target_bandwidth = (config_.max_bandwidth_out / 100.0) * MAX_BANDWIDTH_PERCENTAGE;

	if(out_per_sec > target_bandwidth) {
		// bump up the compression level
	}

	last_bandwidth_out_ = stats.bytes_out;
}

void QoS::shutdown() {
	
}

} // gateway, ember