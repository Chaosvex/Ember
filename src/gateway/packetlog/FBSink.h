/*
 * Copyright (c) 2018 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "PacketSink.h"
#include <shared/util/cstring_view.hpp>
#include <string>
#include <string_view>
#include <fstream>

namespace ember::gateway {

class FBSink final : public PacketSink {
	constexpr static std::uint32_t VERSION = 1;

	std::ofstream file_;
	inline static cstring_view time_fmt_ = "%Y-%m-%dT%H:%M:%SZ"; // ISO 8601

	void start_log(const std::string& filename, std::string_view host, std::string_view remote_host);

public:
	FBSink(const std::string& filename, std::string_view host, std::string_view remote_host);

	void log(std::span<const std::uint8_t> buffer, const std::time_t& time,
	         PacketDirection dir) override;
};

} // gateway, ember