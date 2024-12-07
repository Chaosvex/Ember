/*
 * Copyright (c) 2016 - 2021 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <memory>

namespace ember::gateway {

class WorldConnection final : std::enable_shared_from_this<WorldConnection> {
	boost::asio::ip::tcp::socket socket_;

public:
	explicit WorldConnection(boost::asio::io_context& service) : socket_(service) { }
};

} // gateway, ember