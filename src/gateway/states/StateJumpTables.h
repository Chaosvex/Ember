/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ClientStates.h"
#include "Authentication.h"
#include "CharacterList.h"
#include "SessionClose.h"
#include "WorldForwarder.h"
#include "WorldEnter.h"
#include "WorldTransfer.h"
#include <protocol/Opcodes.h>
#include <array>
#include <algorithm>

#define VALIDATE_JUMP_ENTRIES(x) \
	static_assert(std::ranges::none_of(x, [](auto p) \
		{ return p == nullptr;}), "Missing jump table entry");

namespace ember::gateway { 

struct ClientContext;
struct Event;

using state_func = void(*)(ClientContext&);
using event_handler = void(*)(ClientContext&, const Event*);
using packet_handler = void(*)(ClientContext&, protocol::ClientOpcode);

template<typename T>
using JumpTable = std::array<T, STATES_NUM>;

constexpr JumpTable<event_handler> update_event {
	&authentication::handle_event,
	&character_list::handle_event,
	&world_enter::handle_event,
	&world_transfer::handle_event,
	&world::handle_event,
	&session_close::handle_event
};

constexpr JumpTable<packet_handler> update_packet {
	&authentication::handle_packet,
	&character_list::handle_packet,
	&world_enter::handle_packet,
	&world_transfer::handle_packet,
	&world::handle_packet,
	&session_close::handle_packet
};

constexpr JumpTable<state_func> exit_states {
	&authentication::exit,
	&character_list::exit,
	&world_enter::exit,
	&world_transfer::exit,
	&world::exit,
	&session_close::exit
};

constexpr JumpTable<state_func> enter_states {
	&authentication::enter,
	&character_list::enter,
	&world_enter::enter,
	&world_transfer::enter,
	&world::enter,
	&session_close::enter
};

// ensure jump tables cannot be resized without new entries being added
VALIDATE_JUMP_ENTRIES(enter_states);
VALIDATE_JUMP_ENTRIES(exit_states);
VALIDATE_JUMP_ENTRIES(update_event);
VALIDATE_JUMP_ENTRIES(update_packet);

} // gateway, ember