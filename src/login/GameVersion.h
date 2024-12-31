/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <logger/Logger.h>
#include <format>
#include <cstdint>

namespace ember {

struct GameVersion {
	std::uint8_t major;
	std::uint8_t minor;
	std::uint8_t patch;
	std::uint16_t build;
};

inline std::string to_string(const GameVersion& ver) {
	return std::format("{}.{}.{} ({})", ver.major, ver.minor, ver.patch, ver.build);
}

inline log::Logger& operator<<(log::Logger& stream, GameVersion ver) {
	stream << to_string(ver);
	return stream;
}

inline bool operator==(const GameVersion& lhs, const GameVersion& rhs) {
	return (lhs.major == rhs.major && lhs.minor == rhs.minor && lhs.patch == rhs.patch
	        && lhs.build == rhs.build);
}

inline bool operator>(const GameVersion& lhs, const GameVersion& rhs) {
	if(lhs.major > rhs.major) {
		return true;
	} else if(lhs.major < rhs.major) {
		return false;
	}

	if(lhs.minor > rhs.minor) {
		return true;
	} else if(lhs.minor < rhs.minor) {
		return false;
	}

	if(lhs.patch > rhs.patch) {
		return true;
	} else if(lhs.patch < rhs.patch) {
		return false;
	}

	if(lhs.build > rhs.build) {
		return true;
	} else if(lhs.build < rhs.build) {
		return false;
	}

	return false;
}

inline bool operator!=(const GameVersion& lhs, const GameVersion& rhs) {
	return !(lhs == rhs);
}

inline bool operator<(const GameVersion& lhs, const GameVersion& rhs) {
	return (lhs != rhs && !(lhs > rhs));
}

inline bool operator<=(const GameVersion& lhs, const GameVersion& rhs) {
	return (lhs < rhs || lhs == rhs);
}

inline bool operator>=(const GameVersion& lhs, const GameVersion& rhs) {
	return (lhs > rhs || lhs == rhs);
}

} // ember