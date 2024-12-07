/*
 * Copyright (c) 2018 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "PacketLogger.h"

namespace sc = std::chrono;

namespace ember::gateway {

void PacketLogger::add_sink(std::unique_ptr<PacketSink> sink) {
	sinks_.emplace_back(std::move(sink));
}

void PacketLogger::reset() {
	sinks_.clear();
}

void PacketLogger::log(const spark::io::pmr::Buffer& buffer, std::size_t length, PacketDirection dir) {
	const auto time = sc::system_clock::to_time_t(sc::system_clock::now());

	boost::container::small_vector<std::uint8_t, RESERVE_LEN> out_buf(
		length, boost::container::default_init
	);

	buffer.copy(out_buf.data(), length);

	for(auto& sink : sinks_) {
		sink->log(out_buf, time, dir);
	}
}

void PacketLogger::log(std::span<const std::uint8_t> buffer, PacketDirection dir) {
	const auto time = sc::system_clock::to_time_t(sc::system_clock::now());

	for(auto& sink : sinks_) {
		sink->log(buffer, time, dir);
	}
}

} // gateway, ember