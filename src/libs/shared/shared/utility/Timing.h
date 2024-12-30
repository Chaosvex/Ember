/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <chrono>
#include <functional>
#include <utility>

namespace ember::util {

/*
 * Allows for setting the timer period, ensuring that
 * it's reset to its original value when leaving scope
 */
class ScopedTimerPeriod final {
	using Callback = std::function<void()>;

	Callback cb_;
	bool success_;

public:
	ScopedTimerPeriod(bool success, Callback cb)
		: success_(success),
		  cb_(std::move(cb)) {}

	bool success() const {
		return success_;
	}

	~ScopedTimerPeriod() {
		cb_();
	}
};

ScopedTimerPeriod set_time_period(std::chrono::milliseconds ms);

} // util, ember