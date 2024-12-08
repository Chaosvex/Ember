/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <spark/Peers.h>
#include <spark/RemotePeer.h>
#include <ranges>

namespace ember::spark {

void Peers::add(std::string key, std::shared_ptr<RemotePeer> peer) {
	std::lock_guard guard(lock_);
	peers_.emplace(std::move(key), std::move(peer));
}

void Peers::remove(const std::string& key) {
	std::lock_guard guard(lock_);
	peers_.erase(key);
}

std::shared_ptr<RemotePeer> Peers::find(const std::string& key) {
	std::lock_guard guard(lock_);

	if(auto it = peers_.find(key); it != peers_.end()) {
		return it->second;
	}

	return nullptr;
}

void Peers::notify_remove_handler(Handler* handler) {
	std::lock_guard guard(lock_);

	for(auto& peer : peers_ | std::views::values) {
		peer->remove_handler(handler);
	}
}

} // spark, ember