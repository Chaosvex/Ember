/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "PINAuthenticator.h"
#include <logger/Logger.h>
#include <shared/utility/xoroshiro128plus.h>
#include <shared/utility/base32.h>
#include <boost/assert.hpp>
#include <boost/endian/conversion.hpp>
#include <botan/hash.h>
#include <botan/mac.h>
#include <gsl/gsl_util>
#include <algorithm>
#include <bit>
#include <memory>
#include <utility>
#include <cstddef>
#include <cmath>
#include <ctime>

namespace be = boost::endian;

namespace ember {

PINAuthenticator::PINAuthenticator(const std::uint32_t seed) {
	remap_pin_grid(seed);
}

/*
 * Converts a PIN such as '16785' into an array of bytes
 * {1, 6, 7, 8, 5} used during the hashing process.
 */
void PINAuthenticator::pin_to_bytes(std::uint32_t pin) {
	pin_bytes_.clear();

	while(pin != 0) {
		if(pin_bytes_.size() == pin_bytes_.capacity()) {
			throw std::invalid_argument("Provided PIN was too long");	
		}
	
		pin_bytes_.emplace_back(pin % 10);
		pin /= 10;
	}
	
	if(pin_bytes_.size() < MIN_PIN_LENGTH) {
		throw std::invalid_argument("Provided PIN was too short");
	}

	std::ranges::reverse(pin_bytes_);
}

/* 
 * The client uses the grid seed to remap the numpad layout.
 * The server must use the seed to generate the same layout as the
 * client in order to calculate the expected input sequence from the
 * client. For example, if the user's PIN is '123' and the pad layout is
 * '0, 4, 1, 6, 2, 3' then the expected input sequence becomes '245'.
 */
void PINAuthenticator::remap_pin_grid(std::uint32_t grid_seed) {
	std::array<std::uint8_t, GRID_SIZE> grid { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

	std::uint8_t* remapped_index = remapped_grid.data();

	for(std::size_t i = grid.size(); i > 0; --i) {
		const auto remainder = grid_seed % i;
		grid_seed /= i;
		*remapped_index = grid[remainder];

		std::size_t copy_size = i;
		copy_size -= remainder;
		--copy_size;

		std::uint8_t* src_ptr = grid.data() + remainder + 1;
		std::uint8_t* dst_ptr = grid.data() + remainder;

		std::copy(src_ptr, src_ptr + copy_size, dst_ptr);
		++remapped_index;
	}
}

/* 
 * Takes the user's PIN and the remapped grid to figure out the expected
 * input sequence. That is, calculate the indices of the buttons that the user
 * will press on the game's numpad.
 */
void PINAuthenticator::remap_pin() {
	for(auto& pin_byte : pin_bytes_) {
		const auto index = std::ranges::find(remapped_grid, pin_byte);
		pin_byte = std::distance(remapped_grid.begin(), index);
	}
}

/*
 * Converts the PIN bytes into ASCII values by simply adding 0x30.
 * Mutates the original bytes rather than creating a copy for efficiency.
 * The client processes the digits as ASCII, so we must do the same.
 */
void PINAuthenticator::pin_to_ascii() {
	std::ranges::transform(pin_bytes_, pin_bytes_.begin(), [](auto pin_byte) {
		return pin_byte += 0x30;
	});
}

auto PINAuthenticator::calculate_hash(const SaltBytes& server_salt,
									  const SaltBytes& client_salt,
									  const std::uint32_t pin) -> HashBytes {
	pin_to_bytes(pin); // convert to byte array
	remap_pin();       // calculate the expected input sequence
	pin_to_ascii();

	// x = H(client_salt | H(server_salt | ascii(pin_bytes)))
	HashBytes hash;
	auto hasher = Botan::HashFunction::create_or_throw("SHA-1");
	BOOST_ASSERT_MSG(hasher->output_length() == hash.size(), "Bad hash size");
	hasher->update(server_salt.data(), server_salt.size());
	hasher->update(pin_bytes_.data(), pin_bytes_.size());
	hasher->final(hash.data());

	hasher->update(client_salt.data(), client_salt.size());
	hasher->update(hash.data(), hash.size());
	hasher->final(hash.data());
	return hash;
}

bool PINAuthenticator::validate_pin(const SaltBytes& server_salt,
                                    const SaltBytes& client_salt,
                                    std::span<const std::uint8_t> client_hash,
                                    const std::uint32_t pin) {
	const auto& hash = calculate_hash(server_salt, client_salt, pin);
	return std::ranges::equal(hash, client_hash);
}

std::uint32_t PINAuthenticator::generate_totp_pin(const std::string& secret,
                                                  int interval,
                                                  const util::ClockBase& clock) {
	std::inplace_vector<std::uint8_t, KEY_LENGTH> decoded_key((secret.size() + 7) / 8 * 5);
	const int key_size = base32_decode(reinterpret_cast<const uint8_t*>(secret.data()),
	                                   decoded_key.data(),
	                                   decoded_key.size());

	if(key_size == -1) {
		throw std::invalid_argument("Unable to base32 decode TOTP key, " + secret);
	}

	// not guaranteed by the standard to be the UNIX epoch but it is on all supported platforms
	const auto time = clock.now();
	const auto now = std::chrono::time_point_cast<std::chrono::seconds>(time).time_since_epoch().count();
	auto step = static_cast<std::uint64_t>((std::floor(now / 30))) + interval;

	HashBytes hmac_result;
	auto hmac = Botan::MessageAuthenticationCode::create_or_throw("HMAC(SHA-1)");
	BOOST_ASSERT_MSG(hmac->output_length() == hmac_result.size(), "Bad hash size");
	hmac->set_key(decoded_key.data(), key_size);

	if constexpr(std::endian::native == std::endian::little) {
		hmac->update_be(step);
	} else {
		hmac->update_le(step);
	}

	hmac->final(hmac_result.data());

	const unsigned int offset = hmac_result[19] & 0xF;
	std::uint32_t pin = (hmac_result[offset] & 0x7f) << 24 | (hmac_result[offset + 1] & 0xff) << 16
	                     | (hmac_result[offset + 2] & 0xff) << 8 | (hmac_result[offset + 3] & 0xff);

    be::little_to_native_inplace(pin);

	pin &= 0x7FFFFFFF;
	pin %= 1000000;
	return pin;
}

/*
 * Random number used by the client to 'randomise' the numpad layout.
 * We use this later on to remap our input grid to match that of the client.
 */
std::uint32_t PINAuthenticator::generate_seed() {
	return gsl::narrow_cast<std::uint32_t>(rng::xorshift::next());
}

/* 
 * Returns a completely random 16-byte salt used during hashing
 */
auto PINAuthenticator::generate_salt() -> SaltBytes {
	SaltBytes server_salt;
	std::ranges::generate(server_salt, generate_seed);
	return server_salt;
}

} // ember