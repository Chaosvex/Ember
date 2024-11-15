/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "AccountClient.h"
#include <vector>

namespace ember {

using namespace rpc::Account;

AccountClient::AccountClient(spark::Server& spark, log::Logger& logger)
	: services::AccountClient(spark),
	  logger_(logger) {
	connect("127.0.0.1", 6003); // temp
}

void AccountClient::connect_failed(std::string_view ip, std::uint16_t port) {
	LOG_INFO_ASYNC(logger_, "Failed to connect to account service on {}:{}", ip, port);
}

void AccountClient::on_link_up(const spark::Link& link) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;
	link_ = link;
}

void AccountClient::on_link_down(const spark::Link& link) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;
}

void AccountClient::locate_session(const std::uint32_t account_id, LocateCB cb) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	SessionLookupT msg {
		.account_id = account_id
	};

	send<SessionResponse>(msg, link_, [this, cb = std::move(cb)](auto link, auto message) {
		handle_locate_response(message, cb);
	});
}

void AccountClient::register_session(const std::uint32_t account_id,
                                     const srp6::SessionKey& key,
                                     RegisterCB cb) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	std::vector keyvec(key.t.begin(), key.t.end());

	RegisterSessionT msg {
		.account_id = account_id,
		.key = std::move(keyvec)
	};

	send<RegisterResponse>(msg, link_, [this, cb = std::move(cb)](auto link, auto message) {
		handle_register_response(message, cb);
	});
}

void AccountClient::handle_register_response(std::expected<const RegisterResponse*, spark::Result> res,
                                             const RegisterCB& cb) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	if(!res) {
		cb(Status::RPC_ERROR);
	} else {
		const auto msg = *res;
		cb(msg->status());
	}
}

void AccountClient::handle_locate_response(std::expected<const SessionResponse*, spark::Result> res,
                                           const LocateCB& cb) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	if(!res) {
		cb(Status::RPC_ERROR, {});
		return;
	}
	
	const auto msg = *res;

	if(!msg->key()) {
		cb(msg->status(), {});
		return;
	}

	auto key = Botan::BigInt::decode(msg->key()->data(), msg->key()->size());
	cb(msg->status(), std::move(key));
}

} // ember