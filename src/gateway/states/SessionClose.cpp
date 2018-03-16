/*
 * Copyright (c) 2016 - 2018 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "SessionClose.h"
#include "../ClientHandler.h"

namespace ember::session_close {

void enter(ClientContext* ctx) {
	// don't care, for now
}

void handle_packet(ClientContext* ctx, protocol::ClientOpcode opcode) {
	ctx->handler->packet_skip(*ctx->buffer, opcode);
}

void handle_event(ClientContext* ctx, const Event* event) {
    // don't care, for now
}

void exit(ClientContext* ctx) {
	// don't care, for now
}

} // session_close, ember