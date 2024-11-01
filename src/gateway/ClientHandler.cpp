/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ClientHandler.h"
#include "ClientConnection.h"
#include "Locator.h"
#include "EventDispatcher.h"
#include "states/StateJumpTables.h"
#include "FilterTypes.h"
#include "ClientLogHelper.h"
#include <logger/Logger.h>
#include <protocol/Packets.h>
#include <format>
#include <utility>

namespace ember {

void ClientHandler::start() {
	Locator::dispatcher()->register_handler(this);
	enter_states[context_.state](context_);
}

void ClientHandler::stop() {
	Locator::dispatcher()->remove_handler(this);
	state_update(ClientState::SESSION_CLOSED);
}

void ClientHandler::close() {
	connection_.close_session();
}

void ClientHandler::handle_message(BinaryStream& stream) {
	context_.stream = &stream;
	stream >> opcode_;

	CLIENT_TRACE_FILTER(logger_, LF_NETWORK, context_)
		<< " -> " << protocol::to_string(opcode_) << LOG_ASYNC;

	// handle ping & keep-alive as special cases
	switch(opcode_) {
		case protocol::ClientOpcode::CMSG_PING:
			handle_ping(stream);
			return;
		case protocol::ClientOpcode::CMSG_KEEP_ALIVE: // no response required
			return;
		default:
			break;
	}

	update_packet[context_.state](context_, opcode_);
}

void ClientHandler::handle_event(const Event* event) {
	update_event[context_.state](context_, event);
}

void ClientHandler::handle_event(std::unique_ptr<const Event> event) {
	update_event[context_.state](context_, event.get());
}

void ClientHandler::state_update(ClientState new_state) {
	CLIENT_DEBUG_FILTER(logger_, LF_NETWORK, context_)
		<< "State change, " << ClientState_to_string(context_.state)
		<< " => " << ClientState_to_string(new_state) << LOG_SYNC;

	context_.prev_state = context_.state;
	context_.state = new_state;
	exit_states[context_.prev_state](context_);
	enter_states[context_.state](context_);
}

void ClientHandler::skip(BinaryStream& stream) {
	CLIENT_DEBUG_FILTER(logger_, LF_NETWORK, context_)
		<< ClientState_to_string(context_.state) << " skipping "
		<< protocol::to_string(opcode_)
		<< " (" << std::to_underlying(opcode_) << ")" << LOG_ASYNC;

	stream.skip(stream.read_limit() - stream.total_read());
}

void ClientHandler::handle_ping(BinaryStream& stream) {
	LOG_TRACE_FILTER(logger_, LF_NETWORK) << log_func << LOG_ASYNC;

	protocol::CMSG_PING packet;

	if(!deserialise(packet, stream)) {
		return;
	}

	protocol::SMSG_PONG response;
	response->sequence_id = packet->sequence_id;
	connection_.latency(packet->latency);
	connection_.send(response);
}

void ClientHandler::start_timer(const std::chrono::milliseconds& time) {
	timer_.expires_from_now(time);

	timer_.async_wait([uuid = uuid_](const boost::system::error_code& ec) {
		if(!ec) {
			Event event { EventType::TIMER_EXPIRED };
			Locator::dispatcher()->post_event(uuid, event);
		}
	});
}

void ClientHandler::stop_timer() {
	timer_.cancel();
}

/*
 * Helper that decides whether to print the IP address or username
 * and IP address in log outputs, based on whether authentication
 * has completed
 */
std::string_view ClientHandler::client_identify() const {	
	if(context_.client_id) {
		if(client_id_ext_.empty()) {
			client_id_ext_ = std::format(
				"{} ({}, {})", client_id_, context_.client_id->username, context_.client_id->id
			);
		}

		return client_id_ext_;
	}

	if(client_id_.empty()) [[unlikely]] {
		client_id_ = connection_.remote_address();
	}

	return client_id_;
}

ClientHandler::ClientHandler(ClientConnection& connection, ClientUUID uuid,
                             executor executor, log::Logger& logger)
                             : context_{},
                               connection_(connection),
                               opcode_{},
                               logger_(logger),
                               uuid_(uuid),
                               timer_(executor) { 
	context_.state = context_.prev_state = ClientState::AUTHENTICATING;
	context_.connection = &connection_;
	context_.handler = this;
}

} // ember