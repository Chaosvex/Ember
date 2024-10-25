/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "HelloService.h"
#include <logger/Logger.h>

namespace ember {

HelloService::HelloService(spark::Server& server)
	: services::HelloService(server) {}

void HelloService::on_link_up(const spark::Link& link) {
	LOG_DEBUG_GLOB << "Server: Link up" << LOG_SYNC;
}

void HelloService::on_link_down(const spark::Link& link) {

}

 auto HelloService::handle_say_hello(const rpc::Hello::HelloRequest& msg,
                                     const spark::Link& link,
                                     const spark::Token& token)
                                     -> std::optional<rpc::Hello::HelloReplyT> {
	LOG_INFO_GLOB << "[HelloService] Received message: " << msg.name()->c_str() << LOG_SYNC;

	if(token.is_nil()) {
		return rpc::Hello::HelloReplyT {
			.message = "Greetings, this is the reply from HelloService!"
		};
	} else {
		return rpc::Hello::HelloReplyT {
			.message = "Greetings, this is a tracked reply from HelloService!"
		};
	}
}

}