/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <boost/uuid/uuid.hpp>
#include <memory>
#include <map>

namespace ember::gateway {

class ClientConnection;

class WorldClients final {
	struct Client {
		unsigned map_id;
		std::shared_ptr<ClientConnection> connection;
	};

	std::map<boost::uuids::uuid, Client> clients_; // todo, use a concurrent map

public:
	void add(boost::uuids::uuid uuid, const std::shared_ptr<ClientConnection>& client);
	void remove(boost::uuids::uuid uuid);
	void remove(const std::shared_ptr<ClientConnection>& client);
	std::shared_ptr<ClientConnection> locate(boost::uuids::uuid uuid) const;

};

} // gateway, ember