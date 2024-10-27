/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <bit>
#include <concepts>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <climits>
#include <cstdint>
#include <cstddef>

namespace ember {

template<typename T>
concept is_scoped_enum = std::is_scoped_enum<T>::value;

class FNVHash final {
	static constexpr std::uint32_t INITIAL = 0x811C9DC5;
	std::uint32_t hash_ = INITIAL;

	template<std::integral T>
	std::uint32_t update_l2r(T data) {
		const auto shift = ((sizeof(data) - 1) * CHAR_BIT);

		for(std::size_t i = 0; i < sizeof(data); ++i) {
			update_byte(data >> (shift - (CHAR_BIT * i)) & 0xff);
		}

		return hash_;
	}

	template<std::integral T>
	std::uint32_t update_r2l(T data) {
		for(std::size_t i = 0; i < sizeof(data); ++i) {
			update_byte(data >> (CHAR_BIT * i) & 0xff);
		}

		return hash_;
	}

public:
	template<typename It>
	std::uint32_t update(It begin, const It end) {
		for(; begin != end; ++begin) {
			hash_ = (hash_ * 0x1000193) ^ static_cast<unsigned char>(*begin);
		}

		return hash_;
	}

	std::uint32_t update_byte(std::uint8_t value) {
		hash_ = (hash_ * 0x1000193) ^ value;
		return hash_;
	}

	template<std::integral T>
	std::uint32_t update(T data) {
		return update_l2r(data);
	}

	template<std::integral T>
	std::uint32_t update_le(T native_val) {
		if constexpr(std::endian::native == std::endian::little) {
			return update_r2l(native_val);
		} else {
			return update_l2r(native_val);
		}
	}

	template<std::integral T>
	std::uint32_t update_be(T native_val) {
		if constexpr(std::endian::native == std::endian::little) {
			return update_l2r(native_val);
		} else {
			return update_r2l(native_val);
		}
	}

	template<is_scoped_enum T>
	std::uint32_t update(T data) {
		return update(std::to_underlying(data));
	}

	template<typename T>
	std::uint32_t update(std::span<T> span) {
		return update(span.begin(), span.end());
	}

	std::uint32_t update(const char* t) {
		for(std::size_t i = 0u;; ++i) {
			if(t[i] != '\0') {
				update(t[i]);
			} else {
				break;
			}
		}

		return hash_;
	}

	std::uint32_t update(const std::string& t) {
		return update(t.begin(), t.end());
	}

	std::uint32_t update(std::string_view t) {
		return update(t.begin(), t.end());
	}

	void reset() {
		hash_ = INITIAL;
	}

	std::uint32_t hash() const {
		return hash_;
	}

	std::uint32_t finalise() {
		std::uint32_t ret = hash_;
		hash_ = INITIAL;
		return ret;
	}
};

} // ember