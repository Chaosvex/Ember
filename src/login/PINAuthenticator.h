/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/util/Clock.h>
#include <shared/util/polyfill/inplace_vector>
#include <array>
#include <span>
#include <string>
#include <cstdint>

namespace ember {

class PINAuthenticator final {
public:
	static constexpr int MIN_PIN_LENGTH =  4;
	static constexpr int MAX_PIN_LENGTH = 10;
	static constexpr int GRID_SIZE      = 10;
	static constexpr int KEY_LENGTH     = 10;
	static constexpr int SALT_LENGTH    = 16;
	static constexpr int HASH_LENGTH    = 20;
	
	using SaltBytes = std::array<std::uint8_t, SALT_LENGTH>;
	using HashBytes = std::array<std::uint8_t, HASH_LENGTH>;

private:
	std::array<std::uint8_t, GRID_SIZE> remapped_grid;
	std::inplace_vector<std::uint8_t, MAX_PIN_LENGTH> pin_bytes_;

	void pin_to_ascii();
	void remap_pin_grid(std::uint32_t seed);
	void pin_to_bytes(std::uint32_t pin);
	void remap_pin();

public:
	explicit PINAuthenticator(std::uint32_t seed);

	HashBytes calculate_hash(const SaltBytes& server_salt,
	                         const SaltBytes& client_salt,
	                         std::uint32_t pin);

	bool validate_pin(const SaltBytes& server_salt,
	                  const SaltBytes& client_salt,
	                  std::span<const std::uint8_t> hash,
	                  std::uint32_t pin);

	static SaltBytes generate_salt();
	static std::uint32_t generate_seed();
	static std::uint32_t generate_totp_pin(const std::string& secret,
	                                       int interval,
	                                       const util::ClockBase& clock = util::Clock());
};

} // ember