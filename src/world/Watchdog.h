/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <logger/Logger.h>
#include <atomic>
#include <chrono>
#include <thread>

namespace ember {

using namespace std::chrono_literals;

/*
 * Used to periodically check whether a loop is still
 * updating, terminatING the process if it detects a
 * potential hang.
 */
class Watchdog final {
	log::Logger& logger_;
	std::chrono::seconds max_idle_;
	std::atomic_bool timeout_;
	std::chrono::steady_clock::time_point prev_;
	std::jthread worker_;

	void monitor(std::stop_token stop);
	void check();

public:
	Watchdog(log::Logger& logger, std::chrono::seconds max_idle);
	~Watchdog();

	void stop();
	void notify();
};

} // ember