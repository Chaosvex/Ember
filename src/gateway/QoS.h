/*
 * Copyright (c) 2016 - 2020 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>

namespace ember::gateway {

struct ServerConfig;
class SessionManager;

class QoS {
	const std::chrono::seconds TIMER_FREQUENCY { 120 };
	const unsigned int MAX_BANDWIDTH_PERCENTAGE { 80 };

	const SessionManager& sessions_;
	const ServerConfig& config_;	
	boost::asio::io_context& service_;
	boost::asio::steady_timer timer_;

	std::size_t last_bandwidth_out_;

	void set_timer();
	void measure_bandwidth();

public:
	QoS(const ServerConfig& config, const SessionManager& sessions, boost::asio::io_context& service) : 
		sessions_(sessions),
		config_(config),
		service_(service),
		timer_(service),
		last_bandwidth_out_(0) { }

	void shutdown();
};

} // gateway, ember