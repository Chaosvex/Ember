/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Timing.h"
#include <gsl/gsl_util>

#ifdef _WIN32
	#include <Windows.h>
	#pragma comment(lib, "Winmm.lib")
#endif

namespace ember::util {

// That's right, this only does something on Windows
// Not required for other platforms but it's nice to abstract it anyway
ScopedTimerPeriod set_time_period(const std::chrono::milliseconds ms) {
#ifdef _WIN32
	const auto count = gsl::narrow<UINT>(ms.count());
	const auto result = timeBeginPeriod(count);

	ScopedTimerPeriod sf(result == TIMERR_NOERROR, [count] {
		// don't care about the result, nothing to do if it fails
		timeEndPeriod(count);
	});

	return sf;
#else
	return { true, [] {} };
#endif
}

} // utils, ember