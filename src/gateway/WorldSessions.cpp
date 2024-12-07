/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "WorldSessions.h"
#include "WorldConnection.h"

namespace ember::gateway {

void WorldSessions::add_world(WorldID id, const std::shared_ptr<WorldConnection>& connection) {

}

void WorldSessions::remove_world(WorldID id) {

}

void WorldSessions::remove_world(const std::shared_ptr<WorldConnection>& connection) {

}

std::shared_ptr<WorldConnection> WorldSessions::locate_world(WorldID id) const {
	return nullptr;
}


} // gateway, ember