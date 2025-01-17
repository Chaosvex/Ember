/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "RealmList.h"
#include <logger/Logger.h>
#include <RealmClientStub.h>
#include <unordered_map>
#include <string>
#include <cstdint>

namespace ember {

class RealmClient final : public services::RealmClient {
	RealmList& realmlist_;
	log::Logger& logger_;
	std::unordered_map<std::uint32_t, std::string> realms_;

	void on_link_up(const spark::Link& link) override;
	void on_link_down(const spark::Link& link) override;
	void connect_failed(std::string_view ip, std::uint16_t port) override;

	void request_realm_status(const spark::Link& link);
	void mark_realm_offline(const spark::Link& link);

	void handle_get_status_response(const spark::Link& link, const rpc::Realm::Status& msg) override;
	bool validate_status(const rpc::Realm::Status& msg) const;
	void update_realm(const rpc::Realm::Status& msg);

public:
	RealmClient(spark::Server& server, RealmList& realmlist, log::Logger& logger);
};

} // ember