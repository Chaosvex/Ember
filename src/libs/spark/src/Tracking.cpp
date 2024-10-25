/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <spark/Tracking.h>
#include <spark/Common.h>
#include <logger/Logger.h>
#include <shared/FilterTypes.h>
#include <algorithm>
#include <condition_variable>
#include <functional>
#include <memory>

namespace sc = std::chrono;
using namespace std::chrono_literals;

namespace ember::spark {

Tracking::Tracking(boost::asio::io_context& ctx, log::Logger* logger)
	: timer_(ctx),
	  logger_(logger) {
	start_timer();
}

void Tracking::start_timer() {
	timer_.expires_from_now(frequency_);
	timer_.async_wait(std::bind_front(&Tracking::expired, this));
}

void Tracking::expired(const boost::system::error_code& ec) {
	if(ec == boost::asio::error::operation_aborted) {
		return;
	}

	for(auto it = requests_.begin(); it != requests_.end();) {
		auto& [_, request] = *it;
		request.ttl -= frequency_;

		if(request.ttl <= 0s) {
			timeout(request);
			it = requests_.erase(it);
		} else {
			++it;
		}
	}

	start_timer();
}

void Tracking::on_message(const Link& link,
                          std::span<const std::uint8_t> data,
                          const Token& token) {
	auto it = requests_.find(token);

	// request has already expired or never existed
	if(it == requests_.end()) {
		LOG_DEBUG_FILTER(logger_, LF_SPARK)
			<< "[spark] Received invalid or expired tracked response"
			<< LOG_ASYNC;
		return;
	}

	auto& [_, request] = *it;
	request.state(link, data);
	requests_.erase(it);
}

void Tracking::track(Token token, TrackedState state, sc::seconds ttl) {
	Request request {
		.token = token,
		.state = std::move(state),
		.ttl = ttl
	};

	requests_.emplace(token, std::move(request));
}

void Tracking::timeout(Request& request) {
	spark::Link link; // todo
	request.state(link, std::unexpected(Result::TIMED_OUT));
}

void Tracking::cancel(Request& request) {
	spark::Link link; // todo
	request.state(link, std::unexpected(Result::CANCELLED));
}

Tracking::~Tracking() {
	shutdown();
}

void Tracking::shutdown() {
	timer_.cancel();

	for(auto& [_, request] : requests_) {
		cancel(request);
	}
}

} // spark, ember