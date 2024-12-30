/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "GameVersion.h"
#include "grunt/Magic.h"
#include "shared/utility/FNVHash.h"
#include <boost/unordered/unordered_flat_map.hpp>
#include <optional>
#include <span>
#include <string_view>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace ember {

namespace detail {

struct Key {
	std::uint16_t build;
	grunt::Platform platform;
	grunt::System os;

	bool operator==(const Key& key) const {
		return key.build == build
			&& key.platform == platform
			&& key.os == os;
	}
};

struct KeyHash {
	std::size_t operator()(const Key& key) const {
		FNVHash hasher;
		hasher.update(key.build);
		hasher.update(key.platform);
		return hasher.update(key.os);
	}
};

} // detail

class IntegrityData final {
	boost::unordered_flat_map<detail::Key, std::vector<std::byte>, detail::KeyHash> data_;

	void load_binaries(std::string_view path, std::uint16_t build,
	                   std::span<const std::string_view> files,
	                   grunt::System system, grunt::Platform platform);

public:
	void add_version(const GameVersion& version, std::string_view path);
	void add_versions(std::span<const GameVersion> versions, std::string_view path);
	std::optional<std::span<const std::byte>> lookup(GameVersion version,
	                                                 grunt::Platform platform,
	                                                 grunt::System os) const;
};

} // ember
