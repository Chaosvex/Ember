/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "HelloClient.h"
#include <iostream>
#include <logger/Logger.h>

namespace ember {

HelloClient::HelloClient(spark::Server& spark)
	: services::HelloClient(spark),
	  spark_(spark) {
	connect("127.0.0.1", 8000);
}

void HelloClient::on_link_up(const spark::Link& link) {
	LOG_DEBUG_GLOB << "Client: Link up" << LOG_SYNC;
	say_hello(link);
	say_hello_tracked(link);
}

void HelloClient::on_link_down(const spark::Link& link) {
	LOG_TRACE_GLOB << log_func << LOG_SYNC;
}

void HelloClient::say_hello(const spark::Link& link) {
	rpc::Hello::HelloRequestT msg;
	msg.name = "Aloha from the HelloClient!";
	send(msg, link);
}

void HelloClient::handle_tracked_reply(
	const spark::Link& link,
    std::expected<const rpc::Hello::HelloReply*, spark::Result> msg) {
	LOG_TRACE_GLOB << log_func << LOG_SYNC;
	
	if(msg) {
		LOG_INFO_GLOB << "Tracked response: " << (*msg)->message()->c_str() << LOG_SYNC;
	} else {
		LOG_INFO_GLOB << "Tracked request failed" << LOG_SYNC;
	}
}

void HelloClient::say_hello_tracked(const spark::Link& link) {
	LOG_TRACE_GLOB << log_func << LOG_SYNC;

	rpc::Hello::HelloRequestT msg {
		.name = "This is a tracked request"
	};
	
	send<rpc::Hello::HelloReply>(msg, link, [this](auto link, auto message) {
		handle_tracked_reply(link, message);
	});
}

void HelloClient::handle_say_hello_response(const spark::Link& link, 
                                            const rpc::Hello::HelloReply& msg) {
	LOG_INFO_GLOB << "[HelloClient] Received response: " << msg.message()->c_str() << LOG_SYNC;
}

}