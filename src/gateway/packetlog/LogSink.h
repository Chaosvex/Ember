/*
 * Copyright (c) 2018 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "PacketSink.h"
#include <logger/Logger.h>

namespace ember::gateway {

class LogSink final : public PacketSink {
	log::Logger& logger_;
	const log::Severity severity_;
	const std::string remote_host_;

	void start_log();

public:
	LogSink(log::Logger& logger, log::Severity severity, std::string remote_host);

	void log(std::span<const std::uint8_t> buffer, const std::time_t& time,
	         PacketDirection dir) override;

	~LogSink() override;
};

} // gateway, ember