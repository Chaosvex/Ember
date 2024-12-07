/*
 * Copyright (c) 2018 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "LogSink.h"
#include "../FilterTypes.h"
#include <shared/util/cstring_view.hpp>
#include <shared/util/FormatPacket.h>
#include <logger/Utility.h>
#include <memory>
#include <string_view>
#include <cstddef>

namespace ember::gateway {

LogSink::LogSink(log::Logger& logger, log::Severity severity, std::string remote_host)
                 : logger_(logger), severity_(severity), remote_host_(std::move(remote_host)) {
	start_log();
}

void LogSink::start_log() {
	logger_ << severity_ << log::Filter(LF_PACKET_LOG)
		<< "Starting packet logging for " << remote_host_ << log::flush;
}

void LogSink::log(std::span<const std::uint8_t> buffer, const std::time_t& time,
                  PacketDirection dir) {
	const auto output = util::format_packet(buffer.data(), buffer.size());

	cstring_view fmt("%H:%M:%S");
	std::string_view direction = dir == PacketDirection::INBOUND? "inbound" : "outbound";
	std::tm tm;

#if _MSC_VER && !__INTEL_COMPILER
	localtime_s(&tm, &time);
#else
	localtime_r(&time, &tm);
#endif

	logger_ << severity_ << log::Filter(LF_PACKET_LOG)
		<< "[" << log::detail::put_time(tm, fmt) << "] " << remote_host_ << ", " << direction
		<< ":\n" << output << log::flush;
}

LogSink::~LogSink() {
	logger_ << severity_ << log::Filter(LF_PACKET_LOG)
		<< "Ending packet logging for " << remote_host_ << log::flush;
}

} // gateway, ember