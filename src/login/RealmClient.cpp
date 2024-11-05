/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "RealmClient.h"
#include <algorithm>

namespace ember {

using namespace rpc::Realm;

RealmClient::RealmClient(spark::Server& server, RealmList& realmlist, log::Logger& logger)
	: services::RealmClient(server),
	  realmlist_(realmlist),
	  logger_(logger) {
	connect("127.0.0.1", 8002); // temp
}

void RealmClient::on_link_up(const spark::Link& link) {
	LOG_DEBUG_ASYNC(logger_, "Link up: {}", link.peer_banner);
	request_realm_status(link);
}

void RealmClient::on_link_down(const spark::Link& link) {
	LOG_DEBUG_ASYNC(logger_, "Link closed: {}", link.peer_banner);
	mark_realm_offline(link);
}

void RealmClient::connect_failed(std::string_view ip, const std::uint16_t port) {
	LOG_DEBUG_ASYNC(logger_, "Failed to connect to realm on {}:{}", ip, port);
}

void RealmClient::request_realm_status(const spark::Link& link) {
	RequestStatusT msg{};
	send(msg, link);
}

void RealmClient::mark_realm_offline(const spark::Link& link) {
	auto it = std::ranges::find_if(realms_, [&](const auto& val) {
		return val.second == link.peer_banner;
	});

	// if there's no realm associated with this peer, return
	if(it == realms_.end()) {
		return;
	}

	const auto& [realm_id, _] = *it;
	std::optional<Realm> realm = realmlist_.get_realm(realm_id);
	assert(realm);
	realm->flags |= Realm::Flags::OFFLINE;
	realmlist_.add_realm(*realm);
	LOG_INFO_ASYNC(logger_, "Set realm {} to offline", realm->name);
}

bool RealmClient::validate_status(const Status& msg) const {
	return msg.name() && msg.id() && msg.ip() && msg.address();
}

void RealmClient::update_realm(const Status& msg) {
	// update everything rather than bothering to only set changed fields
	Realm realm {
		.id = msg.id(),
		.name = msg.name()->str(),
		.ip = msg.ip()->str(),
		.port = msg.port(),
		.address = msg.address()->str(),
		.population = msg.population(),
		.type = static_cast<Realm::Type>(msg.type()),
		.flags = static_cast<Realm::Flags>(msg.flags()),
		.category = static_cast<dbc::Cfg_Categories::Category>(msg.category()),
		.region = static_cast<dbc::Cfg_Categories::Region>(msg.region())
	};

	LOG_INFO_ASYNC(logger_, "Updating status for realm {} ({}, {})", realm.id, realm.name, realm.address);
	realmlist_.add_realm(std::move(realm));
}

void RealmClient::handle_get_status_response(const spark::Link& link, const Status& msg) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	if(!validate_status(msg)) {
		LOG_WARN_ASYNC(logger_, "Incompatible realm update from {}", link.peer_banner);
		return;
	}

	// realm may have gone down unexpectedly and restarted before the prior link has terminated
	if(auto it = realms_.find(msg.id()); it != realms_.end()) {
		if(link.peer_banner != it->second) {
			LOG_WARN_ASYNC(logger_, "Realm associated with {} will now be associated with {}",
			               it->second, link.peer_banner);
		}
	}

	update_realm(msg);

	// keep track of this realm's associated peer
	realms_[msg.id()] = link.peer_banner;
}

} // ember