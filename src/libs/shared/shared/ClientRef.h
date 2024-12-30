/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/utility/FNVHash.h>
#include <shared/utility/xoroshiro128plus.h>
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

class ClientRef final {
	static constexpr std::size_t UUID_SIZE = 16;
	static constexpr std::size_t SERVICE_BYTE = 0;

	std::array<std::uint8_t, UUID_SIZE> data_;

	void generate(const std::size_t service_index) {
		for(std::size_t i = 0; i < sizeof(data_); ++i) {
			data_[i] = gsl::narrow_cast<std::uint8_t>(rng::xorshift::next());
		}

		data_[SERVICE_BYTE] = gsl::narrow<std::uint8_t>(service_index);
	}

public:
	explicit ClientRef(std::size_t service_index) {
		generate(service_index);
	}

	explicit ClientRef(std::span<const std::uint8_t> data) {
		if(data.size() != data_.size()) {
			throw std::invalid_argument("bad client data size");
		}

		std::ranges::copy(data, data_.data());
	}

	inline std::size_t hash() const {
		FNVHash hasher;
		return hasher.update(data_.begin(), data_.end());
	}

	inline std::uint8_t service() const {
		return data_[SERVICE_BYTE];
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

	static constexpr auto size() {
		return UUID_SIZE;
	}

	friend bool operator==(const ClientRef& rhs, const ClientRef& lhs);
};

inline bool operator==(const ClientRef& rhs, const ClientRef& lhs) {
	return rhs.hash() == lhs.hash();
}

inline std::size_t hash_value(const ClientRef& uuid) {
	return uuid.hash();
}

} // ember

template <>
struct std::hash<ember::ClientRef> {
	std::size_t operator()(const ember::ClientRef& uuid) const {
		return uuid.hash();
	}
};
