/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "LoginHandler.h"
#include "NetworkSession.h"
#include "SocketType.h"
#include "grunt/Packet.h"
#include "grunt/Handler.h"
#include <logger/LoggerFwd.h>
#include <shared/threading/ThreadPool.h>
#include <spark/buffers/pmr/Buffer.h>
#include <memory>

namespace ember {

class LoginHandlerBuilder;
class ThreadPool;

class LoginSession final : public NetworkSession<LoginSession> {
	ThreadPool& pool_;
	LoginHandler handler_;
	log::Logger& logger_;
	grunt::Handler grunt_handler_;

	void async_completion(Action& action);
	void write_packet(const grunt::Packet& packet, WriteCallback&& cb);
	void execute_async(std::unique_ptr<Action> action);

public:
	LoginSession(SessionManager& sessions, tcp_socket socket, log::Logger& logger,
	             ThreadPool& pool, const LoginHandlerBuilder& builder);

	bool handle_packet(spark::io::pmr::Buffer& buffer);
};

} // ember