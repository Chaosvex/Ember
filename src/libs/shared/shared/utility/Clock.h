/*
 * Copyright (c) 2021 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <chrono>

/* 
 * This is intended to be used to allow for easier unit testing of functions
 * that use the system clock but need to be set to fixed points during tests.
 */

namespace ember::util {

struct ClockBase {
	virtual std::chrono::time_point<std::chrono::system_clock> now() const = 0;
	virtual ~ClockBase() = default;
};

struct Clock final : ClockBase {
	virtual std::chrono::time_point<std::chrono::system_clock> now() const override {
		return std::chrono::system_clock::now();
	}
};

} // util, ember