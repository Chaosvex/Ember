/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <chrono>

namespace ember {

class DeltaTimer final {
	const std::chrono::nanoseconds interval_;
	std::chrono::nanoseconds elapsed_;

public:
	explicit DeltaTimer(std::chrono::nanoseconds interval);

	const std::chrono::nanoseconds& value() const;
	const std::chrono::nanoseconds& interval() const;

	/*
	 * Update the timer with the current frame diff/delta.
	 * Expects a positive value.
	 * Returns whether the timer has elapsed.
	 */
	bool update(const std::chrono::nanoseconds& delta);

	/*
	 * Returns whether the timer has elapsed 
	 */
	bool elapsed() const;

	/*
	 * Intended to act as the condition for a loop body
	 * that may need to run multiple times if the elapsed
	 * time is more than twice the timer's deadline, e.g.
	 * if there's a slow update and a system needs to catch up.
	 * 
	 * Returns true if the timer has elapsed, false otherwise.
	 */
	bool consume();

	/*
	 * Resets the elapsed time back to zero 
	 */
	void reset();
};

} // ember