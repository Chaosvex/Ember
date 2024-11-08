/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/Realm.h>
#include <RealmServiceStub.h>
#include <logger/Logger.h>
#include <mutex>
#include <vector>

namespace ember {

class RealmService final : public services::RealmService {
	std::vector<spark::Link> links_;

	Realm realm_;
	log::Logger& logger_;
	std::mutex mutex;

	rpc::Realm::StatusT status();

	std::optional<rpc::Realm::StatusT> handle_get_status(
		const rpc::Realm::RequestStatus& msg,
		const spark::Link& link,
		const spark::Token& token) override;

	void on_link_up(const spark::Link& link) override;
	void on_link_down(const spark::Link& link) override;
	void broadcast_status();

public:
	RealmService(spark::Server& server, Realm realm, log::Logger& logger);

	void set_online();
	void set_offline();
};

} // ember