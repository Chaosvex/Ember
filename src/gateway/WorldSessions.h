/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <memory>
#include <unordered_map>

namespace ember::gateway {

class WorldConnection;

class WorldSessions final {
	struct WorldID {
		unsigned int realm_id;
		unsigned int map_id;
		unsigned int instance_id;
	};

	std::unordered_map<WorldID, std::shared_ptr<WorldConnection>> connections_; // todo, change, concurrent

public:
	void add_world(WorldID id, const std::shared_ptr<WorldConnection>& connection);
	void remove_world(WorldID id);
	void remove_world(const std::shared_ptr<WorldConnection>& connection);
	std::shared_ptr<WorldConnection> locate_world(WorldID id) const;
};

} // gateway, ember