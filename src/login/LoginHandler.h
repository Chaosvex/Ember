/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Actions.h"
#include "Authenticator.h"
#include "GameVersion.h"
#include "LoginHandlerFwd.h"
#include "LoginState.h"
#include "PINAuthenticator.h"
#include "grunt/PacketFwd.h"
#include "grunt/client/LoginChallenge.h"
#include "grunt/ResultCodes.h"
#include <logger/LoggerFwd.h>
#include <botan/bigint.h>
#include <array>
#include <fstream>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <optional>
#include <unordered_map>
#include <utility>
#include <variant>

namespace ember {

struct TransferState {
	std::ifstream file;
	std::uint64_t offset;
	std::uint64_t size;
	bool abort;
};

class LoginHandler final {
	using CharacterCount = std::unordered_map<std::uint32_t, std::uint32_t>;

	using StateContainer = std::variant<
		std::monostate,
		CharacterCount,
		LoginAuthenticator,
		ReconnectAuthenticator
	>;

	LoginState state_ { LoginState::CHALLENGE };
	Metrics& metrics_;
	log::Logger& logger_;
	const Patcher& patcher_;
	const RealmList& realm_list_;
	const dal::UserDAO& user_src_;
	const std::string source_ip_;
	const AccountClient& acct_svc_;
	const IntegrityData& bin_data_;
	const Survey& survey_;

	StateContainer state_data_;
	std::optional<User> user_;
	Botan::BigInt server_proof_;
	std::array<std::uint8_t, CHECKSUM_SALT_LEN> checksum_salt_;
	PINAuthenticator::SaltBytes pin_salt_;
	std::uint32_t pin_grid_seed_;
	grunt::client::LoginChallenge challenge_;
	TransferState transfer_state_;
	const bool locale_enforce_;
	const bool integrity_enforce_;

	void initiate_login(const grunt::Packet& packet);
	void initiate_file_transfer(const FileMeta& meta);

	void handle_login_proof(const grunt::Packet& packet);
	void handle_reconnect_proof(const grunt::Packet& packet);
	void handle_survey_result(const grunt::Packet& packet);
	void handle_transfer_ack(const grunt::Packet& packet, bool survey);
	void handle_transfer_abort();

	void send_login_challenge(const FetchUserAction& action);
	void send_login_proof(grunt::Result result, bool survey = false);
	void send_reconnect_challenge(const FetchSessionKeyAction& action);
	void send_reconnect_proof(grunt::Result result);
	void send_realm_list(const grunt::Packet& packet);
	grunt::server::LoginChallenge build_login_challenge();

	void on_character_data(const FetchCharacterCounts& action);
	void on_session_write(const RegisterSessionAction& action);
	void on_survey_write(const SaveSurveyAction& action);

	void transfer_chunk();
	void set_transfer_offset(const grunt::Packet& packet);

	bool validate_pin(const grunt::client::LoginProof& packet) const;
	bool validate_protocol_version(const grunt::client::LoginChallenge& challenge) const;

	bool validate_client_integrity(std::span<const std::uint8_t> client_hash,
								   const Botan::BigInt& client_salt,
	                               bool reconnect) const;

	bool validate_client_integrity(std::span<const std::uint8_t> client_hash,
	                               std::span<const std::uint8_t> client_salt,
	                               bool reconnect) const;

	void fetch_user(grunt::Opcode opcode, const utf8_string& username);
	void fetch_session_key(const FetchUserAction& action);

	void reject_client(const GameVersion& version);
	void patch_client(const grunt::client::LoginChallenge& version);

	void update_state(const LoginState& state);

public:
	std::function<void(std::unique_ptr<Action> action)> execute_async;
	std::function<void(const grunt::Packet&)> send;
	std::function<void(const grunt::Packet&, std::function<void()>)> send_cb;

	bool update_state(const Action& action);
	bool update_state(const grunt::Packet& packet);
	void on_chunk_complete();

	LoginHandler(const dal::UserDAO& users, const AccountClient& acct_svc, const Patcher& patcher,
	             const IntegrityData& bin_data, const Survey& survey, log::Logger& logger,
	             const RealmList& realm_list, std::string source, Metrics& metrics,
	             bool locale_enforce, bool integrity_enforce)
	             : user_src_(users), patcher_(patcher), logger_(logger), acct_svc_(acct_svc),
	               realm_list_(realm_list), source_ip_(std::move(source)), metrics_(metrics),
	               bin_data_(bin_data), survey_(survey), transfer_state_{},
	               locale_enforce_(locale_enforce), integrity_enforce_(integrity_enforce),
	               pin_grid_seed_(0) { }
};

} // ember