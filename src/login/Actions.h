/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "AccountClient.h"
#include "grunt/Packet.h"
#include <shared/database/objects/User.h>
#include <shared/database/daos/UserDAO.h>
#include <exception>
#include <future>
#include <string>
#include <optional>
#include <utility>
#include <cstdint>

namespace ember {

class Action {
public:
	virtual void execute() = 0;
	virtual ~Action() = default;
};

class RegisterSessionAction final : public Action {
	const AccountClient& account_svc_;
	std::uint32_t account_id_;
	srp6::SessionKey key_;

	std::promise<rpc::Account::Status> promise_;
	rpc::Account::Status res_;
	std::exception_ptr exception_;

	std::future<rpc::Account::Status> do_register() {
		account_svc_.register_session(account_id_, key_, [&](rpc::Account::Status res) {
			promise_.set_value(res);
		});

		return promise_.get_future();
	}

public:
	RegisterSessionAction(const AccountClient& account_svc, std::uint32_t account_id, srp6::SessionKey key)
		: account_svc_(account_svc),
		  account_id_(account_id),
		  key_(std::move(key)),
		  res_(rpc::Account::Status::UNKNOWN_STATUS) { }

	virtual void execute() override try {
		res_ = do_register().get();
	} catch(const std::exception&) {
		exception_ = std::current_exception();
	}

	rpc::Account::Status get_result() const {
		if(exception_) {
			std::rethrow_exception(exception_);
		}

		return res_;
	}
};

class FetchSessionKeyAction final : public Action {
	const AccountClient& account_svc_;
	std::uint32_t account_id_;
	Botan::BigInt key_;
	std::exception_ptr exception_;

	std::promise<std::pair<rpc::Account::Status, Botan::BigInt>> promise_;
	std::pair<rpc::Account::Status, Botan::BigInt> res_;

	auto do_fetch() {
		account_svc_.locate_session(account_id_, [&](rpc::Account::Status res,
		                            Botan::BigInt key) {
			promise_.set_value({res, key});
		});

		return promise_.get_future();
	}

public:
	FetchSessionKeyAction(const AccountClient& account_svc, std::uint32_t account_id)
		: account_svc_(account_svc),
		  account_id_(account_id) {}

	virtual void execute() override try {
		res_ = do_fetch().get();
	} catch(const std::exception&) {
		exception_ = std::current_exception();
	}

	auto get_result() const {
		if(exception_) {
			std::rethrow_exception(exception_);
		}

		return res_;
	}
};

class FetchUserAction final : public Action {
	const utf8_string username_;
	const dal::UserDAO& user_src_;
	std::optional<User> user_;
	std::exception_ptr exception_;

public:
	FetchUserAction(utf8_string username, const dal::UserDAO& user_src)
		: username_(std::move(username)),
		  user_src_(user_src) {}

	virtual void execute() override try {
		user_ = user_src_.user(username_);
	} catch(const dal::exception&) {
		exception_ = std::current_exception();
	}

	std::optional<User> get_result() const {
		if(exception_) {
			std::rethrow_exception(exception_);
		}

		return user_;
	}

	const std::string& username() const {
		return username_;
	}
};

class FetchCharacterCounts final : public Action {
	const std::uint32_t user_id_;
	const dal::UserDAO& user_src_;
	std::unordered_map<std::uint32_t, std::uint32_t> counts_;
	bool reconnect_;
	std::exception_ptr exception_;

public:
	FetchCharacterCounts(std::uint32_t user_id, const dal::UserDAO& user_src, bool reconnect = false)
		: user_id_(user_id),
		  user_src_(user_src),
		  reconnect_(reconnect) {}

	virtual void execute() override try {
		counts_ = user_src_.character_counts(user_id_);
	} catch(const dal::exception&) {
		exception_ = std::current_exception();
	}

	std::unordered_map<std::uint32_t, std::uint32_t> get_result() const {
		if(exception_) {
			std::rethrow_exception(exception_);
		}

		return counts_;
	}

	bool reconnect() const {
		return reconnect_;
	}
};

class SaveSurveyAction final : public Action {
	const dal::UserDAO& user_src_;
	std::uint32_t user_id_;
	std::uint32_t survey_id_;
	std::string data_;
	dal::exception exception_;
	bool error_;

public:
	SaveSurveyAction(std::uint32_t user_id, const dal::UserDAO& user_src, std::uint32_t survey_id,
	                 std::string data)
		: user_src_(user_src),
		  user_id_(user_id),
		  survey_id_(survey_id),
		  data_(std::move(data)),
		  error_(false) { }

	virtual void execute() override try {
		user_src_.save_survey(user_id_, survey_id_, data_);
	} catch(const dal::exception& e) {
		exception_ = e;
		error_ = true;
	}

	bool error() const {
		return error_;
	}

	dal::exception exception() const {
		return exception_;
	}
};

} // ember