/*
 * Copyright (c) 2021 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <spark/Link.h>
#include <spark/Common.h>
#include <string>
#include <string_view>
#include <span>
#include <cstdint>

namespace ember::spark {

class Handler {
public:
	virtual std::string type() = 0;
	virtual std::string name() = 0;
	virtual void on_message(const spark::Link& link,
	                        std::span<const std::uint8_t> msg,
	                        const spark::Token& token) = 0;
	virtual void on_link_up(const spark::Link& link) = 0;
	virtual void on_link_down(const spark::Link& link) = 0;
	virtual void connect_failed(std::string_view ip, std::uint16_t port) = 0;

	virtual ~Handler() = default;
};

} // spark, ember