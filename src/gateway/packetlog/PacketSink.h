/*
 * Copyright (c) 2018 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <span>
#include <cstddef>
#include <cstdint>
#include <ctime>

namespace ember::gateway {

enum class PacketDirection {
	INBOUND, OUTBOUND
};

class PacketSink {
public:
	virtual void log(std::span<const std::uint8_t> buffer,
	                 const std::time_t& time,
	                 PacketDirection dir) = 0;

	virtual ~PacketSink() = default;
};

} // gateway, ember