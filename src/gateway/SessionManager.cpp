/*
 * Copyright (c) 2015 - 2018 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "SessionManager.h"
#include "ConnectionStats.h"

namespace ember::gateway {

void SessionManager::start(std::unique_ptr<ClientConnection> session) {
	auto sess_ptr = session.get();

	{
		std::lock_guard guard(sessions_lock_);
		sessions_.insert(std::move(session));
	}

	sess_ptr->start();
}

void SessionManager::stop(ClientConnection* session) {
	std::lock_guard guard(sessions_lock_);

	auto it = sessions_.find(session);
	
	if(it == sessions_.end()) {
		return;
	}

	auto client = std::move(sessions_.extract(it).value());
	ClientConnection::async_shutdown(std::move(client));
}

void SessionManager::stop_all() {
	std::lock_guard guard(sessions_lock_);

	while(!sessions_.empty()) {
		auto client = std::move(sessions_.extract(sessions_.begin()).value());
		ClientConnection::async_shutdown(std::move(client));
	}
}

std::size_t SessionManager::count() const {
	return sessions_.size();
}

ConnectionStats SessionManager::aggregate_stats() const {
	std::lock_guard guard(sessions_lock_);

	ConnectionStats ag_stats {};

	for(const auto& session : sessions_) {
		const auto& stats = session->stats();
		ag_stats.bytes_in += stats.bytes_in;
		ag_stats.bytes_out += stats.bytes_out;
		ag_stats.latency += stats.latency;
		ag_stats.messages_in += stats.messages_in;
		ag_stats.messages_out += stats.messages_out;
		ag_stats.packets_in += stats.packets_in;
		ag_stats.packets_out += stats.packets_out;
	}

	ag_stats.latency /= count(); // average latency
	return ag_stats;
}

SessionManager::~SessionManager() {
	stop_all();
}

} // gateway, ember