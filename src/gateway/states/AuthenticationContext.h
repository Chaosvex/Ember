/*
 * Copyright (c) 2020 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <protocol/Packets.h>
#include <cstdint>

namespace ember::gateway::authentication {

enum class State {
	NOT_AUTHED, IN_PROGRESS, IN_QUEUE, SUCCESS, FAILED
};

struct Context {
	State state;
	std::uint32_t seed;
	std::uint32_t account_id;
	protocol::CMSG_AUTH_SESSION packet;
};

} // authentication, gateway, ember