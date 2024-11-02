/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Watchdog.h"
#include <shared/threading/Utility.h>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <cassert>

using namespace std::chrono_literals;

namespace ember {

Watchdog::Watchdog(std::chrono::seconds max_idle, log::Logger& logger)
	: max_idle_(std::move(max_idle)),
	  logger_(logger),
	  timeout_(false),
	  prev_(std::chrono::steady_clock::now()),
	  worker_(std::jthread(std::bind_front(&Watchdog::run, this))) { 
	if(max_idle_ <= 0s) {
		throw std::runtime_error("max_idle must be > 0");
	}

	thread::set_name(worker_, "Watchdog");
}

void Watchdog::run(const std::stop_token token) {
	LOG_DEBUG_ASYNC(logger_, "Watchdog active ({} frequency)", max_idle_);

	std::mutex mutex;
	auto cond_var = std::condition_variable_any();

	while(!token.stop_requested()) {
		check_timeout();
		cond_var.wait_for(mutex, token, max_idle_, [] { return false; });
	}

	LOG_DEBUG_ASYNC(logger_, "Watchdog stopped");
}

void Watchdog::check_timeout() {
	const auto curr = std::chrono::steady_clock::now();
	const auto delta = curr - prev_;

	// guard against spurious wake up
	if(delta < max_idle_) {
		return;
	}

	if(timeout_) {
		timeout(delta);
	}

	prev_ = curr;
	timeout_ = true;
}

void Watchdog::timeout(const std::chrono::nanoseconds& delta) {
	LOG_FATAL_SYNC(logger_, "Watchdog triggered after {}, terminating...",
	               std::chrono::duration_cast<std::chrono::seconds>(delta));
	std::abort();
}

void Watchdog::notify() {
	timeout_ = false;
}

void Watchdog::stop() {
	worker_.request_stop();
}

Watchdog::~Watchdog() {
	stop();
}

} // ember