/*
 * Copyright (c) 2016 - 2020 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/smartenum.hpp>

namespace ember::gateway {

smart_enum(ClientState, int,
	AUTHENTICATING,
	CHARACTER_LIST,
	WORLD_ENTER,
	WORLD_TRANSFER,
	WORLD_FORWARD,
	SESSION_CLOSED
)

constexpr auto STATES_MAX = SESSION_CLOSED;
constexpr auto STATES_NUM = STATES_MAX + 1;

} // gateway, ember