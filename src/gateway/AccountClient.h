/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <Accountv2ClientStub.h>
#include <logger/Logger.h>
#include <spark/v2/Server.h>
#include <botan/bigint.h>
#include <functional>
#include <cstdint>

namespace ember {

class AccountClient final : public services::Accountv2Client {
public:
	using LocateCB = std::function<void(messaging::Accountv2::Status, Botan::BigInt)>;
	using AccountCB = std::function<void(messaging::Accountv2::Status, std::uint32_t)>;

private:
	log::Logger& logger_;
	spark::v2::Link link_;

	void on_link_up(const spark::v2::Link& link) override;
	void on_link_down(const spark::v2::Link& link) override;

	void handle_locate_response(
		std::expected<const messaging::Accountv2::SessionResponse*, spark::v2::Result> resp,
		const LocateCB& cb
	) const;

	void handle_lookup_response(
		std::expected<const messaging::Accountv2::AccountFetchResponse*, spark::v2::Result> resp,
		const AccountCB& cb
	) const;

public:
	AccountClient(spark::v2::Server& spark, log::Logger& logger);

	void locate_session(std::uint32_t account_id, LocateCB cb) const;
	void locate_account_id(const std::string& username, AccountCB cb) const;
};

} // ember