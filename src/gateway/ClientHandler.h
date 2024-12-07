/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Event.h"
#include "FilterTypes.h"
#include "ConnectionDefines.h"
#include "states/ClientContext.h"
#include <protocol/Packet.h>
#include <spark/buffers/BinaryStream.h>
#include <logger/LoggerFwd.h>
#include <shared/ClientRef.h>
#include <boost/asio/steady_timer.hpp>
#include <boost/uuid/uuid.hpp>
#include <concepts>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace ember::gateway {

class ClientConnection;

class ClientHandler final {
	ClientConnection& connection_;
	ClientContext context_;
	const ClientRef uuid_;
	boost::asio::steady_timer timer_;
	protocol::ClientOpcode opcode_;
	log::Logger& logger_;

	mutable std::string client_id_;
	mutable std::string client_id_ext_;

	void handle_ping(BinaryStream& stream);

public:
	ClientHandler(ClientConnection& connection, ClientRef uuid,
	              executor executor, log::Logger& logger);

	void start();
	void stop();
	void close();
	std::string_view client_identify() const;

	template<protocol::is_packet T>
	std::optional<T> deserialise(BinaryStream& stream);

	bool deserialise(protocol::is_packet auto& packet, BinaryStream& stream);
	void skip(BinaryStream& stream);

	void state_update(ClientState new_state);
	void handle_message(BinaryStream& stream);
	void handle_event(const Event* event);
	void handle_event(std::unique_ptr<const Event> event);

	void start_timer(const std::chrono::milliseconds& time);
	void stop_timer();

	const ClientRef& uuid() const {
		return uuid_;
	}
};

#include "ClientHandler.inl"

} // gateway, ember