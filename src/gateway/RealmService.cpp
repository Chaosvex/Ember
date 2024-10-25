/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "RealmService.h"

namespace ember {

using namespace rpc::Realm;
using namespace spark;

RealmService::RealmService(Server& server, Realm realm, log::Logger& logger)
	: services::RealmService(server),
	  realm_(realm),
	  logger_(logger) { }

void RealmService::on_link_up(const Link& link) {
	LOG_DEBUG_ASYNC(logger_, "Link up: {}", link.peer_banner);

	std::lock_guard guard(mutex);
	links_.emplace_back(link);
}

void RealmService::on_link_down(const Link& link) {
	LOG_DEBUG_ASYNC(logger_, "Link closed: {}", link.peer_banner);

	std::lock_guard guard(mutex);

	if(auto it = std::ranges::find(links_, link); it != links_.end()) {
		links_.erase(it);
	}
}

StatusT RealmService::status() {
	return StatusT {
		.id = realm_.id,
		.name = realm_.name,
		.ip = realm_.ip,
		.port = realm_.port,
		.address = realm_.address,
		.population = realm_.population,
		.type = std::to_underlying(realm_.type),
		.flags = std::to_underlying(realm_.flags),
		.category = std::to_underlying(realm_.category),
		.region = std::to_underlying(realm_.region),
	};
}

std::optional<StatusT>
RealmService::handle_get_status(const RequestStatus&, const Link& link,	const Token& token) {
	return status();
}

void RealmService::set_online() {
	realm_.flags &= ~Realm::Flags::OFFLINE;
	broadcast_status();
}

void RealmService::set_offline() {
	realm_.flags |= Realm::Flags::OFFLINE;
	broadcast_status();
}

void RealmService::broadcast_status() {
	auto realm_info = status();

	std::lock_guard guard(mutex);

	for(auto& link : links_) {
		send(realm_info, link);
	}
}

} // ember