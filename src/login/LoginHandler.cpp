/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "LoginHandler.h"
#include "AccountClient.h"
#include "ExecutablesChecksum.h"
#include "IntegrityData.h"
#include "LocaleMap.h"
#include "Patcher.h"
#include "RealmList.h"
#include "Survey.h"
#include "grunt/Packets.h"
#include <logger/Logger.h>
#include <shared/database/daos/UserDAO.h>
#include <shared/metrics/Metrics.h>
#include <shared/util/EnumHelper.h>
#include <boost/container/small_vector.hpp>
#include <gsl/gsl_util>
#include <ranges>
#include <stdexcept>

namespace ember {

bool LoginHandler::update_state(const grunt::Packet& packet) try {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	const LoginState prev_state = state_;
	update_state(LoginState::CLOSED);

	switch(prev_state) {
		case LoginState::CHALLENGE:
			initiate_login(packet);
			break;
		case LoginState::PROOF:
			handle_login_proof(packet);
			break;
		case LoginState::RECONNECT_PROOF:
			handle_reconnect_proof(packet);
			break;
		case LoginState::REQUEST_REALMS:
			send_realm_list(packet);
			break;
		case LoginState::SURVEY_INITIATE:
			handle_transfer_ack(packet, true);
			break;
		case LoginState::PATCH_INITIATE:
			handle_transfer_ack(packet, false);
			break;
		case LoginState::SURVEY_TRANSFER:
		case LoginState::PATCH_TRANSFER:
			handle_transfer_abort();
			break;
		case LoginState::SURVEY_RESULT:
			handle_survey_result(packet);
			break;
		case LoginState::CLOSED:
			return false;
		default:
			LOG_DEBUG(logger_) << "Received packet out of sync" << LOG_ASYNC;
			return false;
	}

	return true;
} catch(const std::exception& e) {
	LOG_DEBUG(logger_) << e.what() << LOG_ASYNC;
	update_state(LoginState::CLOSED);
	return false;
}

bool LoginHandler::update_state(const Action& action) try {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	const LoginState prev_state = state_;
	update_state(LoginState::CLOSED);

	switch(prev_state) {
		case LoginState::FETCHING_USER_LOGIN:
			send_login_challenge(static_cast<const FetchUserAction&>(action));
			break;
		case LoginState::FETCHING_USER_RECONNECT:
			fetch_session_key(static_cast<const FetchUserAction&>(action));
			break;
		case LoginState::FETCHING_SESSION:
			send_reconnect_challenge(static_cast<const FetchSessionKeyAction&>(action));
			break;
		case LoginState::WRITING_SESSION:
			on_session_write(static_cast<const RegisterSessionAction&>(action));
			break;
		case LoginState::REQUEST_REALMS:
			on_survey_write(static_cast<const SaveSurveyAction&>(action));
			break;
		case LoginState::FETCHING_CHARACTER_DATA:
			on_character_data(static_cast<const FetchCharacterCounts&>(action));
			break;
		case LoginState::CLOSED:
			return false;
		default:
			LOG_WARN(logger_) << "Received action out of sync" << LOG_ASYNC;
			return false;
	}

	return true;
} catch(const std::exception& e) {
	LOG_DEBUG(logger_) << e.what() << LOG_ASYNC;
	update_state(LoginState::CLOSED);
	return false;
}

void LoginHandler::initiate_login(const grunt::Packet& packet) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	auto& challenge = dynamic_cast<const grunt::client::LoginChallenge&>(packet);

	/* 
	 * Older clients are likely to be using an older protocol version
	 * but they're close enough that patch transfers will still work
	 */
	if(!validate_protocol_version(challenge)) {
		LOG_DEBUG_ASYNC(logger_, "Unsupported protocol version {} ({})",
		                challenge.protocol_ver, source_ip_);
	}

	if(challenge.game != grunt::Game::WoW) {
		LOG_DEBUG_ASYNC(logger_, "Bad game magic ({})", source_ip_);
		update_state(LoginState::CLOSED);
		return;
	}

	LOG_DEBUG_ASYNC(logger_, "Challenge: {}, {} ({})", challenge.username,
	                 to_string(challenge.version), source_ip_);

	const Patcher::PatchLevel level = patcher_.check_version(challenge.version);

	switch(level) {
		case Patcher::PatchLevel::OK:
			fetch_user(challenge.opcode, challenge.username);
			break;
		case Patcher::PatchLevel::TOO_NEW:
			reject_client(challenge.version);
			break;
		case Patcher::PatchLevel::TOO_OLD:
			patch_client(challenge);
			break;
	}

	challenge_ = challenge;
}

bool LoginHandler::validate_protocol_version(const grunt::client::LoginChallenge& challenge) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	const auto version = challenge.protocol_ver;

	if(challenge.opcode == grunt::Opcode::CMD_AUTH_LOGON_CHALLENGE
		&& version == grunt::client::LoginChallenge::CHALLENGE_VER) {
		return true;
	}

	if(challenge.opcode == grunt::Opcode::CMD_AUTH_RECONNECT_CHALLENGE
		&& version == grunt::client::ReconnectChallenge::RECONNECT_CHALLENGE_VER) {
		return true;
	}

	return false;
}

void LoginHandler::fetch_user(grunt::Opcode opcode, const utf8_string& username) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	switch(opcode) {
		case grunt::Opcode::CMD_AUTH_LOGON_CHALLENGE:
			update_state(LoginState::FETCHING_USER_LOGIN);
			break;
		case grunt::Opcode::CMD_AUTH_RECONNECT_CHALLENGE:
			update_state(LoginState::FETCHING_USER_RECONNECT);
			break;
		default:
			update_state(LoginState::CLOSED);
			BOOST_ASSERT_MSG(false, "Impossible fetch_user condition");
	}

	auto action = std::make_unique<FetchUserAction>(username, user_src_);
	execute_async(std::move(action));
}

void LoginHandler::fetch_session_key(const FetchUserAction& action_res) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	if(!(user_ = action_res.get_result())) {
		LOG_DEBUG_ASYNC(logger_, "Account not found: {}", action_res.username());
		return;
	}

	update_state(LoginState::FETCHING_SESSION);
	auto action = std::make_unique<FetchSessionKeyAction>(acct_svc_, user_->id());
	execute_async(std::move(action));
}

void LoginHandler::reject_client(const GameVersion& version) {
	LOG_DEBUG_ASYNC(logger_, "Rejecting client version {}", to_string(version));

	grunt::server::LoginChallenge response;
	response.result = grunt::Result::FAIL_VERSION_INVALID;
	send(response);
}

grunt::server::LoginChallenge LoginHandler::build_login_challenge() {	
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	grunt::server::LoginChallenge packet;
	const auto& authenticator = std::get<LoginAuthenticator>(state_data_);
	const auto& values = authenticator.challenge_reply();
	packet.B = values.B;
	packet.g_len = gsl::narrow<std::uint8_t>(values.gen.generator().bytes());
	packet.g = gsl::narrow<std::uint8_t>(values.gen.generator().to_u32bit());
	packet.n_len = grunt::server::LoginChallenge::PRIME_LENGTH;
	packet.N = values.gen.prime();
	packet.s = values.salt;
	packet.two_factor_auth = false;

	if(user_->pin_method() != PINMethod::NONE) {
		packet.two_factor_auth = true;
		packet.pin_grid_seed = pin_grid_seed_ = PINAuthenticator::generate_seed();
		packet.pin_salt = pin_salt_ = PINAuthenticator::generate_salt();
	}

	Botan::AutoSeeded_RNG().randomize(checksum_salt_.data(), checksum_salt_.size());
	packet.checksum_salt = checksum_salt_;
	return packet;
}

void LoginHandler::send_login_challenge(const FetchUserAction& action) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	grunt::server::LoginChallenge response;

	try {
		if((user_ = action.get_result())) {
			state_data_.emplace<LoginAuthenticator>(*user_);
			response = build_login_challenge();
			response.result = grunt::Result::SUCCESS;
			update_state(LoginState::PROOF);
		} else {
			// leaks information on whether the account exists (could send challenge anyway?)
			response.result = grunt::Result::FAIL_UNKNOWN_ACCOUNT;
			metrics_.increment("login_failure");
			LOG_DEBUG(logger_) << "Account not found: " << action.username() << LOG_ASYNC;
		}
	} catch(dal::exception& e) {
		response.result = grunt::Result::FAIL_DB_BUSY;
		metrics_.increment("login_internal_failure");
		LOG_ERROR_ASYNC(logger_, "DAL failure for {}: {}", action.username(), e.what());
	} catch(Botan::Exception& e) {
		response.result = grunt::Result::FAIL_DB_BUSY;
		metrics_.increment("login_internal_failure");
		LOG_ERROR_ASYNC(logger_, "Encoding failure for {}: {}", action.username(), e.what());
	}
	
	send(response);
}

void LoginHandler::send_reconnect_proof(grunt::Result result) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	LOG_DEBUG_ASYNC(logger_, "Reconnect result for {}: {}", user_->username(), grunt::to_string(result));

	if(result == grunt::Result::SUCCESS) {
		metrics_.increment("login_success");
	} else {
		metrics_.increment("login_failure");
	}

	grunt::server::ReconnectProof response;
	response.result = result;
	send(response);
}

void LoginHandler::send_reconnect_challenge(const FetchSessionKeyAction& action) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	grunt::server::ReconnectChallenge response;
	response.result = grunt::Result::SUCCESS;

	Botan::AutoSeeded_RNG().randomize(checksum_salt_.data(), checksum_salt_.size());
	response.salt = checksum_salt_;

	const auto& [status, key] = action.get_result();

	if(status == rpc::Account::Status::OK) {
		update_state(LoginState::RECONNECT_PROOF);
		state_data_.emplace<ReconnectAuthenticator>(user_->username(), key, checksum_salt_);
	} else if(status == rpc::Account::Status::SESSION_NOT_FOUND) {
		metrics_.increment("login_failure");
		response.result = grunt::Result::FAIL_NOACCESS;
		LOG_DEBUG_ASYNC(logger_, "Reconnect failed, session not found for {}", user_->username());
	} else {
		metrics_.increment("login_internal_failure");
		response.result = grunt::Result::FAIL_DB_BUSY;
		LOG_ERROR_ASYNC(logger_, "{} from peer during reconnect challenge",
		                util::fb_status(status, rpc::Account::EnumNamesStatus()));
	}
	
	send(response);
}

bool LoginHandler::validate_pin(const grunt::client::LoginProof& packet) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	// no PIN was expected, nothing to validate
	if(user_->pin_method() == PINMethod::NONE) {
		return true;
	}

	// PIN auth is enabled for this user, make sure the packet has PIN data
	if(!packet.two_factor_auth) {
		return false;
	}

	PINAuthenticator pin_auth(pin_grid_seed_);
	bool result = false;

	if(user_->pin_method() == PINMethod::TOTP) {
		for(auto interval : {0, -1, 1}) { // try time intervals -1 to +1
			const auto pin = PINAuthenticator::generate_totp_pin(user_->totp_token(), interval);

			if(pin_auth.validate_pin(pin_salt_, packet.pin_salt, packet.pin_hash, pin)) {
				result = true;
				break;
			}
		}
	} else if(user_->pin_method() == PINMethod::FIXED) {
		result = pin_auth.validate_pin(pin_salt_, packet.pin_salt, packet.pin_hash, user_->pin());
	} else {
		LOG_ERROR_ASYNC(logger_, "Unknown TOTP method, {}", std::to_underlying(user_->pin_method()));
	}

	LOG_DEBUG_ASYNC(logger_, "PIN authentication for {} {}", user_->username(), result? "OK" : "failed");
	return result;
}

bool LoginHandler::validate_client_integrity(std::span<const std::uint8_t> hash,
                                             const Botan::BigInt& salt,
                                             bool reconnect) const {
	constexpr auto expected_len = 32u;

	boost::container::small_vector<std::uint8_t, expected_len> bytes(
		salt.bytes(), boost::container::default_init
	);

	salt.binary_encode(bytes.data(), bytes.size());
	std::ranges::reverse(bytes);
	return validate_client_integrity(hash, bytes, reconnect);
}

bool LoginHandler::validate_client_integrity(std::span<const std::uint8_t> client_hash,
                                             std::span<const uint8_t> salt,
                                             bool reconnect) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	if(!integrity_enforce_) {
		return true;
	}

	const auto data = bin_data_.lookup(challenge_.version, challenge_.platform, challenge_.os);

	// ensure we have binaries for the platform/version the client is using
	if(!data) {
		return false;
	}

	constexpr static int SHA1_LENGTH{ 20 }; // it's finally somewhere else
	std::array<std::uint8_t, SHA1_LENGTH> hash{};

	// client doesn't bother to checksum the binaries on reconnect, it just hashes the salt (=])
	if(reconnect) {
		constexpr std::array<std::uint8_t, SHA1_LENGTH> checksum{}; // all-zero hash
		hash = client_integrity::finalise(checksum, salt);
	} else {
		const auto& checksum = client_integrity::checksum(checksum_salt_, *data);
		hash = client_integrity::finalise(checksum, salt);
	}

	return std::ranges::equal(hash, client_hash);
}

void LoginHandler::handle_login_proof(const grunt::Packet& packet) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	auto& proof_packet = dynamic_cast<const grunt::client::LoginProof&>(packet);

	if(!validate_client_integrity(proof_packet.client_checksum, proof_packet.A, false)) {
		send_login_proof(grunt::Result::FAIL_VERSION_INVALID);
		return;
	}

	if(!validate_pin(proof_packet)) {
		send_login_proof(grunt::Result::FAIL_INCORRECT_PASSWORD);
		return;
	}

	const auto& authenticator = std::get<LoginAuthenticator>(state_data_);
	const auto key = authenticator.session_key(proof_packet.A);
	auto proof = authenticator.expected_proof(key, proof_packet.A);
	auto result = grunt::Result::FAIL_INCORRECT_PASSWORD;
	
	if(proof_packet.M1 == proof) {
		if(user_->banned()) {
			result = grunt::Result::FAIL_BANNED;
		} else if(user_->suspended()) {
			result = grunt::Result::FAIL_SUSPENDED;
		} else if(!user_->subscriber()) {
			result = grunt::Result::FAIL_NO_TIME;
		/*} else if(parental_controls) {
			result = grunt::Result::FAIL_PARENTAL_CONTROLS;*/
		} else {
			result = grunt::Result::SUCCESS;
		}
	}

	if(result == grunt::Result::SUCCESS) {
		update_state(LoginState::WRITING_SESSION);
		server_proof_ = authenticator.server_proof(key, proof_packet.A, proof_packet.M1);

		auto action = std::make_unique<RegisterSessionAction>(
			acct_svc_, user_->id(),
			authenticator.session_key(proof_packet.A)
		);

		execute_async(std::move(action));
	} else {
		send_login_proof(result);
	}
}

void LoginHandler::send_login_proof(grunt::Result result, bool survey) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	grunt::server::LoginProof response;
	response.result = result;

	if(result == grunt::Result::SUCCESS) {
		metrics_.increment("login_success");
		response.M2 = server_proof_;
		response.survey_id = survey? survey_.id() : 0;
	} else {
		metrics_.increment("login_failure");
	}

	LOG_DEBUG_ASYNC(logger_, "Login result for {}: {}", user_->username(), grunt::to_string(result));
	send(response);
}

void LoginHandler::on_character_data(const FetchCharacterCounts& action) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	try {
		state_data_ = action.get_result();
	} catch(dal::exception& e) { // not a fatal exception, we'll keep going without the data
		state_data_ = CharacterCount();
		metrics_.increment("login_internal_failure");
		LOG_ERROR_ASYNC(logger_, "DAL failure for {}: {}", user_->username(), e.what());
	}

	update_state(LoginState::REQUEST_REALMS);

	if(action.reconnect()) {
		send_reconnect_proof(grunt::Result::SUCCESS);
		return;
	}
	
	if(user_->survey_request() && survey_.id()
	   && survey_.data(challenge_.platform, challenge_.os)) {
		update_state(LoginState::SURVEY_INITIATE);
	}

	send_login_proof(grunt::Result::SUCCESS, state_ == LoginState::SURVEY_INITIATE);

	if(state_ == LoginState::SURVEY_INITIATE) {
		LOG_DEBUG(logger_) << "Initiating survey transfer..." << LOG_ASYNC;
		auto meta = survey_.meta(challenge_.platform, challenge_.os);
		assert(meta);
		initiate_file_transfer(*meta);
	}
}

void LoginHandler::on_session_write(const RegisterSessionAction& action) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	auto result = action.get_result();
	grunt::Result response = grunt::Result::SUCCESS;

	if(result == rpc::Account::Status::OK) {
		update_state(LoginState::FETCHING_CHARACTER_DATA);
	} else if(result == rpc::Account::Status::ALREADY_LOGGED_IN) {
		response = grunt::Result::FAIL_ALREADY_ONLINE;
	} else {
		metrics_.increment("login_internal_failure");
		response = grunt::Result::FAIL_DB_BUSY;
		LOG_ERROR_ASYNC(logger_, "{} from peer during login",
		                util::fb_status(result, rpc::Account::EnumNamesStatus()));
	}

	// defer sending the response until we've fetched the character data
	if(result == rpc::Account::Status::OK) {
		execute_async(std::make_unique<FetchCharacterCounts>(user_->id(), user_src_));
	} else {
		send_login_proof(response);
	}
}

void LoginHandler::handle_reconnect_proof(const grunt::Packet& packet) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	auto& reconn_proof = dynamic_cast<const grunt::client::ReconnectProof&>(packet);

	if(!validate_client_integrity(reconn_proof.client_checksum, reconn_proof.salt, true)) {
		send_reconnect_proof(grunt::Result::FAIL_VERSION_INVALID);
		return;
	}

	const auto& authenticator = std::get<ReconnectAuthenticator>(state_data_);

	if(authenticator.proof_check(reconn_proof.salt, reconn_proof.proof)) {
		update_state(LoginState::FETCHING_CHARACTER_DATA);
		execute_async(std::make_unique<FetchCharacterCounts>(user_->id(), user_src_, true));
	} else {
		send_reconnect_proof(grunt::Result::FAIL_INCORRECT_PASSWORD);
	}
}

void LoginHandler::send_realm_list(const grunt::Packet& packet) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	// todo - move these checks (inc. dynamic_casts) out of the handler functions
	if(packet.opcode != grunt::Opcode::CMD_REALM_LIST) {
		throw std::runtime_error("Expected CMD_REALM_LIST");
	}

	// look the client's locale up for sending the correct realm category
	auto it = locale_map.find(challenge_.locale);

	if(it == locale_map.end()) {
		return;
	}

	const auto& [_, region] = *it;
	const std::shared_ptr<const RealmMap> realms = realm_list_.realms();
	const auto& char_count = std::get<CharacterCount>(state_data_);
	grunt::server::RealmList response;

	for(const auto& realm : *realms | std::views::values) {
		if(!locale_enforce_ || realm.region == region) {
			if(auto count = char_count.find(realm.id); count != char_count.end()) {
				response.realms.emplace_back(realm, count->second);
			} else {
				response.realms.emplace_back(realm, 0);
			}
		}
	}

	update_state(LoginState::REQUEST_REALMS);
	send(response);
}

void LoginHandler::patch_client(const grunt::client::LoginChallenge& challenge) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	auto meta = patcher_.find_patch(challenge.version, challenge.locale,
	                                challenge.platform, challenge.os);

	if(!meta) {
		reject_client(challenge.version);
		return;
	}

	grunt::server::LoginChallenge response;
	response.result = grunt::Result::FAIL_VERSION_UPDATE;
	send(response);

	auto& fmeta = meta->file_meta;

	LOG_DEBUG(logger_) << "Initiating patch transfer, " << fmeta.name << LOG_ASYNC;
	std::ifstream patch(fmeta.path + fmeta.name, std::ifstream::binary);

	if(!patch) {
		LOG_ERROR_ASYNC(logger_, "Could not open patch, {}", fmeta.name);
		return;
	}

	transfer_state_.file = std::move(patch);
	
	if(meta->mpq) {
		fmeta.name = "Patch";
	}

	metrics_.increment("patches_sent");
	update_state(LoginState::PATCH_INITIATE);
	initiate_file_transfer(fmeta);
}

void LoginHandler::initiate_file_transfer(const FileMeta& meta) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;
	
	transfer_state_.size = meta.size;

	grunt::server::TransferInitiate response;
	response.filename = meta.name;
	response.filesize = meta.size;
	response.md5 = meta.md5;
	send(response);
}

void LoginHandler::handle_survey_result(const grunt::Packet& packet) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	auto& survey = dynamic_cast<const grunt::client::SurveyResult&>(packet);

	// allow the client to request the realmlist without waiting on the survey write callback
	update_state(LoginState::REQUEST_REALMS);

	if(survey.survey_id != survey_.id()) {
		LOG_DEBUG_ASYNC(logger_, "Received an invalid survey ID from {}", user_->username());
		return;
	}

	/*
	 * Errors can be caused by the client having already sent data for the
	 * active survey ID or by the compressed data length being too large
	 * for the client to send (hardcoded to 1000 bytes)
	 */
	if(survey.error) {
		return;
	}

	auto action = std::make_unique<SaveSurveyAction>(
		user_->id(), user_src_,
		survey.survey_id, survey.data
	);

	metrics_.increment("surveys_received");
	execute_async(std::move(action));
}

void LoginHandler::on_survey_write(const SaveSurveyAction& action) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	if(action.error()) {
		LOG_ERROR_ASYNC(logger_, "DAL failure for {}, {}", user_->username(), action.exception().what());
	}
}

void LoginHandler::set_transfer_offset(const grunt::Packet& packet) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	auto& resume = dynamic_cast<const grunt::client::TransferResume&>(packet);
	transfer_state_.offset = resume.offset;
}

void LoginHandler::handle_transfer_ack(const grunt::Packet& packet, bool survey) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	switch(packet.opcode) {
		case grunt::Opcode::CMD_XFER_RESUME:
			set_transfer_offset(packet);
			[[fallthrough]];
		case grunt::Opcode::CMD_XFER_ACCEPT:
			update_state(survey? LoginState::SURVEY_TRANSFER : LoginState::PATCH_TRANSFER);
			transfer_chunk();
			break;
		case grunt::Opcode::CMD_XFER_CANCEL:
			update_state(survey? LoginState::SURVEY_RESULT : LoginState::CLOSED);
			break;
		default:
			update_state(LoginState::CLOSED);
			break;
	}
}

void LoginHandler::handle_transfer_abort() {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;
	transfer_state_.abort = true;
}

void LoginHandler::transfer_chunk() {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	auto remaining = transfer_state_.size - transfer_state_.offset;
	std::uint16_t read_size = grunt::server::TransferData::MAX_CHUNK_SIZE;

	if(read_size > remaining) {
		read_size = gsl::narrow<std::uint16_t>(remaining);
	}

	grunt::server::TransferData response;
	response.size = read_size;

	if(state_ == LoginState::SURVEY_TRANSFER) {
		auto survey_mpq = survey_.data(challenge_.platform, challenge_.os)->begin();
		survey_mpq += gsl::narrow<int>(transfer_state_.offset);
		std::copy(survey_mpq, survey_mpq + read_size, response.chunk.data());
	} else {
		transfer_state_.file.read(reinterpret_cast<char*>(response.chunk.data()), read_size);

		if(!transfer_state_.file.good()) {
			LOG_ERROR(logger_) << "Patch reading failed during transfer" << LOG_ASYNC;
			return;
		}
	}

	transfer_state_.offset += read_size;

	send_cb(response, [&]() {
		on_chunk_complete();
	});
}

void LoginHandler::on_chunk_complete() {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	if(transfer_state_.abort) {
		return;
	}

	// transfer complete?
	if(transfer_state_.offset == transfer_state_.size) {
		switch(state_) {
			case LoginState::SURVEY_TRANSFER:
				update_state(LoginState::SURVEY_RESULT);
				break;
			case LoginState::PATCH_TRANSFER:
				update_state(LoginState::CLOSED);
				break;
			default:
				break;
		}
	} else {
		transfer_chunk();
	}
}

inline void LoginHandler::update_state(const LoginState& state) {
	state_ = state;
}

} // ember
