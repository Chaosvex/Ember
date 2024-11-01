/*
 * Copyright (c) 2021 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Handler.h"
#include <logger/LoggerFwd.h>
#include <memory>

namespace ember::dns {

class Socket;
struct Query;

class Server final : public Handler {
    std::unique_ptr<Socket> socket_;
    log::Logger& logger_;

    void handle_question(const Query& query);
    void handle_response(const Query& query);
    void handle_datagram(std::span<const std::uint8_t> datagram) override;

public:
    Server(std::unique_ptr<Socket> socket, log::Logger& logger);
	~Server();

	void shutdown();
};

} // dns, ember