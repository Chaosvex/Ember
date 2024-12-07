/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ClientContext.h"
#include "../Events.h"
#include <chrono>
#include <memory>

namespace ember::gateway::character_list {

using namespace std::chrono_literals;
constexpr auto CHAR_LIST_TIMEOUT = 15min;

void enter(ClientContext& ctx);
void handle_packet(ClientContext& ctx, protocol::ClientOpcode opcode);
void handle_event(ClientContext& ctx, const Event* event);
void exit(ClientContext& ctx);

} // character_list, gateway, ember