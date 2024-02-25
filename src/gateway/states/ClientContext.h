/*
 * Copyright (c) 2016 - 2020 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ClientStates.h"
#include "AuthenticationContext.h"
#include "WorldEnterContext.h"
#include "../ConnectionDefines.h"
#include <spark/buffers/Buffer.h>
#include <protocol/PacketHeaders.h>
#include <shared/util/UTF8String.h>
#include <optional>
#include <variant>
#include <cstdint>

namespace ember {

class ClientHandler;
class ClientConnection;

struct WorldContext {
	//std::shared_ptr<WorldConnection> world_conn;
};

using StateContext = 
	std::variant<
		authentication::Context,
		world_enter::Context
	>;

struct ClientID {
	std::uint32_t id;
	utf8_string username;
};

struct ClientContext {
	ClientStream* stream;
	ClientState state;
	ClientState prev_state;
	ClientHandler* handler;
	ClientConnection* connection;
	StateContext state_ctx;
	std::optional<ClientID> client_id;
};

} // ember