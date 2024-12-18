/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "grunt/Magic.h"
#include <shared/database/objects/PatchMeta.h>
#include <functional>
#include <span>
#include <string>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cstddef>
#include <cstdint>

namespace ember {

class Survey final {
	struct Key {
		grunt::Platform platform;
		grunt::System os;

		bool operator==(const Key& key) const {
			return key.os == os && key.platform == platform;
		}
	};

	struct KeyHash {
		std::size_t operator()(const Key& key) const {
			using os_t = std::underlying_type<grunt::System>::type;
			using platform_t = std::underlying_type<grunt::Platform>::type;

			return std::hash<os_t>()(std::to_underlying(key.os))
				^ (std::hash<platform_t>()(std::to_underlying(key.platform)) << 1) >> 1;
		}
	};

	const std::uint32_t id_;
	std::unordered_map<Key, FileMeta, KeyHash> meta_;
	std::unordered_map<Key, std::vector<std::byte>, KeyHash> data_;

public:
	explicit Survey(std::uint32_t id = 0) : id_(id) {}

	std::uint32_t id() const;
	void add_data(grunt::Platform platform, grunt::System os, const std::string& path);
	
	[[nodiscard]]
	std::optional<std::reference_wrapper<const FileMeta>>
	meta(grunt::Platform platform, grunt::System os) const;

	[[nodiscard]]
	std::optional<std::span<const std::byte>>
	data(grunt::Platform platform, grunt::System os) const;
};

} // ember