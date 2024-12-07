/*
 * Copyright (c) 2016 - 2022 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "WorldForwarder.h"
#include "../Routing.h"
#include "../FilterTypes.h"
#include "../ClientLogHelper.h"
#include "../ClientHandler.h"
#include <logger/Logger.h>
#include <utility>

namespace ember::gateway::world {

void route_packet(ClientContext& ctx, protocol::ClientOpcode opcode, Route route);

void enter(ClientContext& ctx) {

}

void handle_packet(ClientContext& ctx, protocol::ClientOpcode opcode) {
	if(auto it = cmsg_routes.find(opcode); it != cmsg_routes.end()) {
		auto& [_, route] = *it;
		route_packet(ctx, opcode, route);
	} else {
		CLIENT_DEBUG_FILTER_GLOB(LF_NETWORK, ctx) << "Unroutable message, "
			<< protocol::to_string(opcode) << " (" << std::to_underlying(opcode) << ")"
			<< " from " << ctx.client_id->username << LOG_ASYNC;
	}
}

void handle_event(ClientContext& ctx, const Event* event) {

}

void route_packet(ClientContext& ctx, protocol::ClientOpcode opcode, Route route) {
	// all dressed up and nowhere to go
}

void exit(ClientContext& ctx) {
	//queue_service_temp->free_slot();
}

} // world, gateway, ember