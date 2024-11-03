/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <spark/buffers/pmr/Buffer.h>
#include <gsl/gsl_util>
#include <botan/bigint.h>
#include <boost/assert.hpp>
#include <boost/container/small_vector.hpp>
#include <array>
#include <span>
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace ember {

class PacketCrypto final {
	static constexpr auto KEY_SIZE_HINT = 40u;

	boost::container::small_vector<std::uint8_t, KEY_SIZE_HINT> key_;
	std::uint8_t send_i_ = 0;
	std::uint8_t send_j_ = 0;
	std::uint8_t recv_i_ = 0;
	std::uint8_t recv_j_ = 0;

public:
	explicit PacketCrypto(std::span<const std::uint8_t> key) {
		key_.assign(key.begin(), key.end());

		BOOST_ASSERT_MSG(
			key.size() <= std::numeric_limits<std::uint8_t>::max(),
			"Session key too big"
		);
	}

	explicit PacketCrypto(const Botan::BigInt& key) {
		BOOST_ASSERT_MSG(
			key.bytes() <= std::numeric_limits<std::uint8_t>::max(),
			"Session key too big"
		);

		key_.resize(key.bytes(), boost::container::default_init);
		key.binary_encode(key_.data(), key_.size());
	}

	void encrypt(auto& data) {
		auto d_bytes = reinterpret_cast<std::uint8_t*>(&data);
		const auto key_size = gsl::narrow_cast<std::uint8_t>(key_.size());
	
		for(std::size_t t = 0; t < sizeof(data); ++t) {
			send_i_ %= key_size;
			std::uint8_t x = (d_bytes[t] ^ key_[send_i_]) + send_j_;
			++send_i_;
			d_bytes[t] = send_j_ = x;
		}
	}

	void decrypt(auto& data) {
		decrypt(&data, sizeof(data));
	}

	void decrypt(auto* data, const std::size_t length) {
		const auto key_size = gsl::narrow_cast<std::uint8_t>(key_.size());

		for(std::size_t t = 0; t < length; ++t) {
			recv_i_ %= key_size;
			auto& byte = reinterpret_cast<char&>(data[t]);
			std::uint8_t x = (byte - recv_j_) ^ key_[recv_i_];
			++recv_i_;
			recv_j_ = byte;
			byte = x;
		}
	}
};

} // ember