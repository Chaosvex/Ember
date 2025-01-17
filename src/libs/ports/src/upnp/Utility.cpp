/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <ports/upnp/Utility.h>
#include <gsl/gsl_util>
#include <stdexcept>
#include <cctype>

namespace ember::ports::upnp {

long long span_to_ll(std::span<const char> span) {
	long long value = 0;
	bool negative = false;
	bool first = true;

	for(auto byte : span) {
		if(first && byte == '-') {
			negative = true;
		} else if(std::isdigit(byte) == 0) {
			throw std::invalid_argument("span_to_int: cannot convert");
		} else {
			value = (value * 10) + (static_cast<long long>(byte) - '0');
		}

		first = false;
	}

	return negative? -value : value;
}

/*
  This exists because string_view isn't guaranteed to be null-terminated,
  (and we know ours isn't) so we can't use the standard atoi functions
*/ 
int sv_to_int(std::string_view string) {
	return gsl::narrow<int>(span_to_ll(string));
}

long sv_to_long(std::string_view string) {
	return gsl::narrow<long>(span_to_ll(string));
}

long long sv_to_ll(std::string_view string) {
	return span_to_ll(string);
}

/*
   Just a quick and dirty func. to extract values from HTTP fields (e.g. "max-age=300")
   C++ developers arguing about how best to split strings and on why
   the standard still provides no functionality for it will never not be funny
 */
std::string_view split_argument(std::string_view input, const char needle) {
	const auto pos = input.find_last_of(needle);

	if(!pos) {
		throw std::invalid_argument("split_view, can't find needle");
	}

	if(pos == input.size() - 1) {
		throw std::invalid_argument("split_view, nothing after needle");
	}

	return input.substr(pos + 1, input.size());
}

} // upnp, ports, ember