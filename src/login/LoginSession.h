/*
 * Copyright (c) 2015 - 2021 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "NetworkSession.h"
#include "LoginHandler.h"
#include "grunt/Packet.h"
#include "grunt/Handler.h"
#include <logger/Logging.h>
#include <shared/threading/ThreadPool.h>
#include <spark/buffers/pmr/Buffer.h>
#include <memory>

namespace ember {

class LoginHandlerBuilder;
class ThreadPool;

class LoginSession final : public NetworkSession {
	void async_completion(Action& action);
	void write_chain(const grunt::Packet& packet, bool notify);
	void execute_async(const std::shared_ptr<Action>& action);

public:
	ThreadPool& pool_;
	LoginHandler handler_;
	log::Logger* logger_;
	grunt::Handler grunt_handler_;

	LoginSession(SessionManager& sessions, boost::asio::ip::tcp::socket socket,
	             boost::asio::ip::tcp::endpoint ep, log::Logger* logger,
	             ThreadPool& pool, const LoginHandlerBuilder& builder);

	bool handle_packet(spark::io::pmr::Buffer& buffer) override;
	void on_write_complete() override;
};

} // ember