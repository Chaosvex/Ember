/*
 * Copyright (c) 2016 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cstddef>

namespace ember::gateway {

struct ConnectionStats {
	std::size_t bytes_in;
	std::size_t bytes_out;
	std::size_t messages_in;
	std::size_t messages_out;
	std::size_t packets_in;
	std::size_t packets_out;
	std::size_t latency;
};

} // gateway, ember