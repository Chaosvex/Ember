/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "AccountService.h"

namespace ember {

using namespace ember::rpc::Account;
using namespace spark;

AccountService::AccountService(Server& spark, AccountHandler& handler, Sessions& sessions, log::Logger& logger)
	: handler_(handler), 
	  services::AccountService(spark),
	  sessions_(sessions),
	  logger_(logger) {}

void AccountService::on_link_up(const Link& link) {
	LOG_DEBUG_ASYNC(logger_, "Link up from {}", link.peer_banner);
}

void AccountService::on_link_down(const Link& link) {
	LOG_DEBUG_ASYNC(logger_, "Link down from {}", link.peer_banner);
}

std::optional<SessionResponseT>
AccountService::handle_session_fetch(
	const SessionLookup& msg,
	const Link& link,
	const Token& token) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	SessionResponseT response;

	if(!msg.account_id()) {
		response.status = Status::ILLFORMED_MESSAGE;
		return response;
	}

	const auto session = sessions_.lookup_session(msg.account_id());
		
	if(!session) {
		response.status = Status::SESSION_NOT_FOUND;
		return response;
	}

	auto key = Botan::BigInt::encode(*session);

	response.status = Status::OK;
	response.account_id = msg.account_id();
	response.key = std::move(key);
	return response;
}

std::optional<RegisterResponseT>
AccountService::handle_register_session(const RegisterSession& msg,	const Link& link, const Token& token) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	RegisterResponseT response {
		.status = Status::OK
	};

	if(msg.key() && msg.account_id()) {
		Botan::BigInt key(msg.key()->data(), msg.key()->size());

		if(!sessions_.register_session(msg.account_id(), key)) {
			response.status = Status::ALREADY_LOGGED_IN;
		}
	} else {
		response.status = Status::ILLFORMED_MESSAGE;
	}

	return response;
}

std::optional<AccountFetchResponseT>
AccountService::handle_account_id_fetch(const LookupID& msg, const Link& link, const Token& token) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;
	
	if(!msg.account_name()) {
		return AccountFetchResponseT {
			.status = Status::ILLFORMED_MESSAGE
		};
	}

	handler_.lookup_id(msg.account_name()->str(), [&, link, token](auto result) {
		if(!result) {
			AccountFetchResponseT response {
				.status = Status::UNKNOWN_ERROR
			};

			send(response, link, token);
			return;
		}

		const auto id = *result;

		AccountFetchResponseT response {
			.status = id? Status::OK : Status::ACCOUNT_NOT_FOUND,
			.account_id = id? *id : 0
		};
		
		send(response, link, token);
	});

	return std::nullopt;
}

std::optional<DisconnectSessionResponseT>
AccountService::handle_disconnect_by_session(const DisconnectSession& msg,	const Link& link, const Token& token) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;
	return std::nullopt;
}

std::optional<DisconnectResponseT>
AccountService::handle_disconnect_by_id(const DisconnectID& msg, const Link& link,	const Token& token) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;
	return std::nullopt; 
}

} // ember