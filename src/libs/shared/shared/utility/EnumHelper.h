/*
 * Copyright (c) 2016 - 2022 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <gsl/gsl_util>
#include <string>
#include <type_traits>
#include <utility>
#include <cstddef>

/*
 * Utility functions for printing out enum values returned from remote
 * Spark services using the FlatBuffers-based protocol
 */
namespace ember::util {

// makes sure converting an enum value to a string isn't going to cause a crash
// in the case of a protocol mismatch
const char* safe_print(auto value, const char* const* enums) {
	std::size_t length = 0;
	const char* current = enums[0];

	while(current != nullptr) {
		current = enums[length];
		++length;
	}

	const auto num_val = std::to_underlying(value);

	// can FlatBuffers handle negative enum value printing? Check, todo!
	if(gsl::narrow<std::size_t>(num_val) < length && enums[num_val] && enums[num_val][0] != '\0') {
		return enums[num_val];
	}

	return "<UNKNOWN STATUS STRING>";
}

std::string fb_status(auto value, const char* const* enums) {
	auto message = safe_print(value, enums);
	return std::string(message) + " (" + std::to_string(std::to_underlying(value)) + ")";
}

} // util, ember