/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Authenticator.h"
#include <logger/Logger.h>
#include <srp6/Util.h>
#include <boost/assert.hpp>
#include <botan/hash.h>
#include <algorithm>
#include <utility>

namespace ember {

LoginAuthenticator::LoginAuthenticator(User user)
	: user_(std::move(user)),
	  srp_(gen_, Botan::BigInt(user_.verifier())) {}

auto LoginAuthenticator::challenge_reply() const -> ChallengeResponse {
	Botan::BigInt salt { user_.salt().data(), user_.salt().size_bytes() };
	return { srp_.public_ephemeral(), std::move(salt), gen_ };
}

Botan::BigInt LoginAuthenticator::server_proof(const srp6::SessionKey& key,
                                               const Botan::BigInt& A,
                                               const Botan::BigInt& M1) const {
	return srp_.generate_proof(key, A, M1);
}

Botan::BigInt LoginAuthenticator::expected_proof(const srp6::SessionKey& key,
                                                 const Botan::BigInt& A) const {
	const Botan::BigInt& B = srp_.public_ephemeral();

	return srp6::generate_client_proof(
		user_.username(), key, gen_.prime(), gen_.generator(), A, B, user_.salt()
	);
}

srp6::SessionKey LoginAuthenticator::session_key(const Botan::BigInt& A) const {
	return srp_.session_key(A);
}

ReconnectAuthenticator::ReconnectAuthenticator(utf8_string username,
                                               const Botan::BigInt& session_key,
                                               std::span<const std::uint8_t, CHECKSUM_SALT_LEN> salt)
                                               : username_(std::move(username)) {
	std::ranges::copy(salt, salt_.data());
	sess_key_.t.resize(session_key.bytes());
	session_key.binary_encode(sess_key_.t.data(), sess_key_.t.size());
}

bool ReconnectAuthenticator::proof_check(std::span<const std::uint8_t> salt,
                                         std::span<const std::uint8_t> proof) const {
	std::array<std::uint8_t, 20> res;
	auto hasher = Botan::HashFunction::create_or_throw("SHA-1");
	BOOST_ASSERT_MSG(hasher->output_length() == res.size(), "Bad hash size");
	hasher->update(username_);
	hasher->update(salt.data(), salt.size());
	hasher->update(salt_.data(), salt_.size());
	hasher->update(sess_key_.t.data(), sess_key_.t.size());
	hasher->final(res.data());
	return std::ranges::equal(res, proof);
}

} // ember