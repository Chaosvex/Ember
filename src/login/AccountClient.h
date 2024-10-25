/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <AccountClientStub.h>
#include <logger/Logger.h>
#include <spark/Server.h>
#include <srp6/Util.h>
#include <functional>
#include <cstdint>

namespace ember {

class AccountClient final : public services::AccountClient {
public:
	using RegisterCB = std::function<void(rpc::Account::Status)>;
	using LocateCB = std::function<void(rpc::Account::Status, Botan::BigInt)>;

private:
	log::Logger& logger_;
	spark::Link link_;

	void on_link_up(const spark::Link& link) override;
	void on_link_down(const spark::Link& link) override;
	void connect_failed(std::string_view ip, std::uint16_t port) override;

	void handle_register_response(
		std::expected<const rpc::Account::RegisterResponse*, spark::Result> res,
		const RegisterCB& cb
	) const;

	void handle_locate_response(
		std::expected<const rpc::Account::SessionResponse*, spark::Result> res,
		const LocateCB& cb
	) const;

public:
	AccountClient(spark::Server& spark, log::Logger& logger);

	void register_session(std::uint32_t account_id,
	                      const srp6::SessionKey& key,
	                      RegisterCB cb) const;

	void locate_session(std::uint32_t account_id, LocateCB cb) const;
};

} // ember