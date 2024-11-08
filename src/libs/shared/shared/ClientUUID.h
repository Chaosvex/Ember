/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/util/FNVHash.h>
#include <shared/util/xoroshiro128plus.h>
#include <boost/functional/hash.hpp>
#include <gsl/gsl_util>
#include <algorithm>
#include <array>
#include <span>
#include <iomanip>
#include <string>
#include <sstream>
#include <stdexcept>
#include <cstdint>
#include <cstddef>

namespace ember {

class ClientUUID final {
	static constexpr std::size_t UUID_SIZE = 16;

	mutable std::size_t hash_ = 0;

	union {
		std::array<std::uint8_t, UUID_SIZE> data_;

		struct {
			std::uint8_t service_;
			std::array<std::uint8_t, 15> rand_;
		};
	};

	mutable bool hashed_ = false;
public:
	inline std::size_t hash() const {
		if(!hashed_) {
			FNVHash hasher;
			hash_ = hasher.update(std::begin(data_), std::end(data_));
			hashed_ = true;
		}

		return hash_;
	}

	inline std::uint8_t service() const {
		return service_;
	}

	// don't really care about efficiency here, it's for debugging
	inline std::string to_string() const {
		std::stringstream stream;
		stream << std::hex;

		for(auto byte : data_) {
			stream << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
		}

		return stream.str();
	}

	static ClientUUID from_bytes(std::span<const std::uint8_t> data) {
		ClientUUID uuid;

		if(data.size() != uuid.data_.size()) {
			throw std::invalid_argument("bad client uuid size");
		}

		std::ranges::copy(data, uuid.data_.data());
		return uuid;
	}

	static ClientUUID generate(std::size_t service_index) {
		ClientUUID uuid;

		for(std::size_t i = 0; i < sizeof(data_); ++i) {
			uuid.data_[i] = gsl::narrow_cast<std::uint8_t>(rng::xorshift::next());
		}

		uuid.service_ = gsl::narrow<std::uint8_t>(service_index);
		return uuid;
	}

	static constexpr auto size() {
		return UUID_SIZE;
	}

	friend bool operator==(const ClientUUID& rhs, const ClientUUID& lhs);
};

inline bool operator==(const ClientUUID& rhs, const ClientUUID& lhs) {
	return rhs.hash() == lhs.hash();
}

inline std::size_t hash_value(const ClientUUID& uuid) {
	return uuid.hash();
}

} // ember

template <>
struct std::hash<ember::ClientUUID> {
	std::size_t operator()(const ember::ClientUUID& uuid) const {
		return uuid.hash();
	}
};
