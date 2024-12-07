/*
 * Copyright (c) 2020 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cstdint>

namespace ember::gateway::world_enter {

enum class State {
	INITIATED
};

struct Context {
	State state { State::INITIATED };
	std::uint64_t character_id {};
};

} // world_enter, gateway, ember