/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Sessions.h"
#include "AccountHandler.h"
#include <AccountServiceStub.h>
#include <logger/Logger.h>

namespace ember {

class AccountService final : public services::AccountService {
	AccountHandler& handler_;
	Sessions& sessions_;
	log::Logger& logger_;

	std::optional<rpc::Account::SessionResponseT> handle_session_fetch(
		const rpc::Account::SessionLookup& msg,
		const spark::Link& link,
		const spark::Token& token) override;

	std::optional<rpc::Account::RegisterResponseT> handle_register_session(
		const rpc::Account::RegisterSession& msg,
		const spark::Link& link,
		const spark::Token& token) override;

	std::optional<rpc::Account::AccountFetchResponseT> handle_account_id_fetch(
		const rpc::Account::LookupID& msg,
		const spark::Link& link,
		const spark::Token& token) override;

	std::optional<rpc::Account::DisconnectSessionResponseT> handle_disconnect_by_session(
		const rpc::Account::DisconnectSession& msg,
		const spark::Link& link,
		const spark::Token& token) override;

	std::optional<rpc::Account::DisconnectResponseT> handle_disconnect_by_id(
		const rpc::Account::DisconnectID& msg,
		const spark::Link& link,
		const spark::Token& token) override;

	void on_link_up(const spark::Link& link) override;
	void on_link_down(const spark::Link& link) override;

public:
	AccountService(spark::Server& spark, AccountHandler& handler,
	               Sessions& sessions, log::Logger& logger);
};

} // ember