/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <spark/Link.h>
#include <spark/Common.h>
#include <logger/LoggerFwd.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/functional/hash.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/uuid/uuid.hpp>
#include <chrono>
#include <cstdint>

namespace ember::spark {

class Tracking final {
	struct Request {
		Token token;
		TrackedState state;
		std::chrono::seconds ttl;
	};

	boost::unordered_flat_map<boost::uuids::uuid, Request,
	                          boost::hash<boost::uuids::uuid>> requests_;

	std::chrono::seconds frequency_ { 5 }; // temp
	boost::asio::steady_timer timer_;
	log::Logger& logger_;

	void start_timer();
	void expired(const boost::system::error_code& ec);
	void timeout(Request& request);
	void cancel(Request& request);

public:
	Tracking(boost::asio::io_context& io_context, log::Logger& logger);
	~Tracking();

	void track(Token token, TrackedState state, std::chrono::seconds ttl);
	void on_message(const Link& link, std::span<const std::uint8_t> data, const Token& token);
	void shutdown();
};

} // spark, ember