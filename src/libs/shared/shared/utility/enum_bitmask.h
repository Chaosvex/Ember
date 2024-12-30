/*
 * Copyright (c) 2016 - 2022 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <concepts>
#include <type_traits>

namespace ember {

template<typename Enum>
struct EnableBitMask {
	static constexpr bool value = false;
};

template<typename T>
concept bitmask_enum = EnableBitMask<T>::value && std::is_scoped_enum<T>::value;

template<bitmask_enum Enum>
Enum operator|(Enum lhs, Enum rhs) {
	using underlying = typename std::underlying_type<Enum>::type;
	return static_cast<Enum> (
		static_cast<underlying>(lhs) |
		static_cast<underlying>(rhs)
	);
}

template<bitmask_enum Enum>
Enum operator&(Enum lhs, Enum rhs) {
	using underlying = typename std::underlying_type<Enum>::type;
	return static_cast<Enum> (
		static_cast<underlying>(lhs) &
		static_cast<underlying>(rhs)
	);
}

template<bitmask_enum Enum>
Enum operator^(Enum lhs, Enum rhs) {
	using underlying = typename std::underlying_type<Enum>::type;
	return static_cast<Enum> (
		static_cast<underlying>(lhs) ^
		static_cast<underlying>(rhs)
	);
}

template<bitmask_enum Enum>
Enum operator~(Enum rhs) {
	using underlying = typename std::underlying_type<Enum>::type;
	return static_cast<Enum> (
		~static_cast<underlying>(rhs)
	);
}

template<bitmask_enum Enum>
Enum operator|=(Enum &lhs, Enum rhs) {
	using underlying = typename std::underlying_type<Enum>::type;
	lhs = static_cast<Enum> (
		static_cast<underlying>(lhs) |
		static_cast<underlying>(rhs)
	);

	return lhs;
}

template<bitmask_enum Enum>
Enum operator&=(Enum &lhs, Enum rhs) {
	using underlying = typename std::underlying_type<Enum>::type;
	lhs = static_cast<Enum> (
		static_cast<underlying>(lhs) &
		static_cast<underlying>(rhs)
	);

	return lhs;
}

template<bitmask_enum Enum>
Enum operator^=(Enum &lhs, Enum rhs) {
	using underlying = typename std::underlying_type<Enum>::type;

	lhs = static_cast<Enum> (
		static_cast<underlying>(lhs) ^
		static_cast<underlying>(rhs)
	);

	return lhs;
}

#define ENABLE_BITMASK(x)                \
template<>                               \
struct EnableBitMask<x>                  \
{                                        \
    static constexpr bool value = true; \
};

} // ember
