/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "GameVersion.h"
#include "PatchGraph.h"
#include "grunt/Magic.h"
#include <shared/database/daos/PatchDAO.h>
#include <shared/database/objects/PatchMeta.h>
#include <shared/utility/FNVHash.h>
#include <logger/Logger.h>
#include <span>
#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <cstddef>

namespace ember {

class Patcher final {
	struct Key {
		std::string_view locale;
		std::string_view platform;
		std::string_view os;

		bool operator==(const Key& key) const {
			return key.locale == locale
				&& key.platform == platform
				&& key.os == os;
		}
	};

	struct KeyHash {
		std::size_t operator()(const Key& key) const {
			FNVHash hasher;
			hasher.update(key.locale);
			hasher.update(key.platform);
			return hasher.update(key.os);
		}
	};

	const std::vector<PatchMeta> patches_;
	const std::vector<GameVersion> versions_;
	std::unordered_map<Key, PatchGraph, KeyHash> graphs_;
	std::unordered_map<Key, std::vector<PatchMeta>, KeyHash> patch_bins;

	const PatchMeta* locate_rollup(std::span<const PatchMeta> patches,
	                               std::uint16_t from, std::uint16_t to) const;

	static void load_patch(PatchMeta& patch, const dal::PatchDAO& dao, const std::string& path);

public:
	enum class PatchLevel { OK, TOO_OLD, TOO_NEW };

	Patcher(std::vector<GameVersion> versions, std::vector<PatchMeta> patches);
	
	std::optional<PatchMeta> find_patch(const GameVersion& client_version,
	                                    grunt::Locale locale,
	                                    grunt::Platform platform,
	                                    grunt::System os) const;

	PatchLevel check_version(const GameVersion& client_version) const;

	static std::vector<PatchMeta> load_patches(const std::string& path, const dal::PatchDAO& dao);
};

} // ember