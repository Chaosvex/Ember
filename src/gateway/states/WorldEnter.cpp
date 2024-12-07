/*
 * Copyright (c) 2020 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "WorldEnter.h"

namespace ember::gateway::world_enter {

void initiate_player_login(ClientContext& ctx, const PlayerLogin* event) {
    auto& state_ctx = std::get<Context>(ctx.state_ctx);
    state_ctx.character_id = event->character_id_;
}

void enter(ClientContext& ctx) {
    ctx.state_ctx = Context{};
}

void handle_packet(ClientContext& ctx, protocol::ClientOpcode opcode) {

}

void handle_event(ClientContext& ctx, const Event* event) {
    switch(event->type) {
        case EventType::PLAYER_LOGIN:
            initiate_player_login(ctx, static_cast<const PlayerLogin*>(event));
            break;
        default:
            break;
    }
}

void exit(ClientContext& ctx) {

}

} // world_enter, gateway, ember