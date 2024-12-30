/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <srp6/Server.h>
#include <shared/database/objects/User.h>
#include <shared/utility/UTF8String.h>
#include <array>
#include <span>

namespace ember {

constexpr std::size_t CHECKSUM_SALT_LEN = 16u; // todo, UZ when supported across the board

class ReconnectAuthenticator final {
	utf8_string username_;
	std::array<std::uint8_t, CHECKSUM_SALT_LEN> salt_;
	srp6::SessionKey sess_key_;

public:
	ReconnectAuthenticator(utf8_string username,
	                       const Botan::BigInt& session_key,
	                       std::span<const std::uint8_t, CHECKSUM_SALT_LEN> salt);

	bool proof_check(std::span<const std::uint8_t> salt,
	                 std::span<const std::uint8_t> proof) const;

	const utf8_string& username() const { return username_; }
};

class LoginAuthenticator final {
	const static inline srp6::Generator gen_ { srp6::Generator::Group::_256_BIT };

	struct ChallengeResponse {
		const Botan::BigInt& B;
		Botan::BigInt salt;
		const srp6::Generator& gen;
	};

	User user_;
	srp6::Server srp_;

public:
	explicit LoginAuthenticator(User user);

	ChallengeResponse challenge_reply() const;

	Botan::BigInt server_proof(const srp6::SessionKey& key,
	                           const Botan::BigInt& A,
	                           const Botan::BigInt& M1) const;

	Botan::BigInt expected_proof(const srp6::SessionKey& key,
	                             const Botan::BigInt& A) const;

	srp6::SessionKey session_key(const Botan::BigInt& A) const;
};

} // ember