/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Patcher.h"
#include "PatchGraph.h"
#include <shared/util/FileMD5.h>
#include <boost/endian/conversion.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cassert>

namespace ember {

Patcher::Patcher(std::vector<GameVersion> versions, std::vector<PatchMeta> patches)
                 : patches_(std::move(patches)), versions_(std::move(versions)) {
	for(auto& patch : patches_) {
		const Key key {
			.locale = patch.locale,
			.platform = patch.arch,
			.os = patch.os
		};

		patch_bins[key].emplace_back(patch);
	}

	for(auto& [size, meta] : patch_bins) {
		graphs_.emplace(size, PatchGraph(meta));
	}
}

const PatchMeta* Patcher::locate_rollup(std::span<const PatchMeta> patches,
                                        std::uint16_t from, std::uint16_t to) const {
	const PatchMeta* meta = nullptr;

	for(auto& patch : patches) {
		if(!patch.rollup) {
			continue;
		}

		// rollup build must be <= the client build and <= the server build
		if(patch.build_from <= from && patch.build_to <= to) {
			if(meta == nullptr) {
				meta = &patch;
			} else if(meta->file_meta.size >= patch.file_meta.size) {
				meta = &patch; // go for the smaller file
			}
		}
	}

	return meta;
}

std::optional<PatchMeta> Patcher::find_patch(const GameVersion& client_version,
                                             const grunt::Locale locale,
                                             const grunt::Platform platform,
                                             const grunt::System os) const {
	const Key key {
		.locale = grunt::to_string(locale),
		.platform = grunt::to_string(platform),
		.os = grunt::to_string(os)
	};
	
	auto g_it = graphs_.find(key);
	auto p_it = patch_bins.find(key);
	
	if(g_it == graphs_.end() || p_it == patch_bins.end()) {
		return std::nullopt;
	}

	auto build = client_version.build;
	bool path = false;

	// ensure there's a patch path from the client version to a supported version
	for(const auto& version : versions_) {
		if(g_it->second.is_path(build, version.build)) {
			path = true;
			break;
		}
	}

	// couldn't find a patch path, find the best rollup patch that'll cover the client
	if(!path) {
		for(auto& version : versions_) {
			const auto meta = locate_rollup(p_it->second, client_version.build, version.build);

			// check to see whether there's a patch path from this rollup
			if(meta && g_it->second.is_path(meta->build_from, version.build)) {
				build = meta->build_from;
				path = true;
				break;
			}
		}

		// still no path? Guess we're out of luck.
		if(!path) {
			return std::nullopt;
		}
	}

	// using the optimal patching path, locate the next patch file
	for(auto& version : versions_) {
		auto paths = g_it->second.path(build, version.build);
		
		if(paths.empty()) {
			continue;
		}
		
		auto build_to = version.build;
		auto build_from = paths.front().from;
		paths.pop_front();

		if(!paths.empty()) {
			build_to = paths.front().from;
		}

		for(const auto& patch : p_it->second) {
			if(patch.build_from == build_from && patch.build_to == build_to) {
				return patch;
			}
		}
	}

	return std::nullopt;
}

auto Patcher::check_version(const GameVersion& client_version) const -> PatchLevel {
	if(std::ranges::find(versions_, client_version) != versions_.end()) {
		return PatchLevel::OK;
	}

	// Figure out whether any of the allowed client versions are newer than the client.
	// If so, there's a chance that it can be patched.
	for(const auto& v : versions_) {
		if(v > client_version) {
			return PatchLevel::TOO_OLD;
		}
	}

	return PatchLevel::TOO_NEW;
}

std::vector<PatchMeta> Patcher::load_patches(const std::string& path, const dal::PatchDAO& dao) {
	auto patches = dao.fetch_patches();

	for(auto& patch : patches) {
		bool dirty = false;
		patch.file_meta.path = path;

		// we open each patch to make sure that it at least exists
		std::ifstream file(path + patch.file_meta.name, std::ios::binary);

		if(!file) {
			throw std::runtime_error("Error opening patch " + path + patch.file_meta.name);
		}

		if(patch.file_meta.size == 0) {
			std::error_code ec{};
			const auto size = std::filesystem::file_size(path + patch.file_meta.name);

			if(ec) {
				throw std::runtime_error("Unable determine patch size for " + path + patch.file_meta.name);
			}

			patch.file_meta.size = static_cast<std::uint64_t>(size);
			dirty = true;
		}

		// check whether the hash is all zeroes and calculate it if so
		const auto calc_md5 = std::ranges::all_of(patch.file_meta.md5, [](const auto& byte) {
			return byte == 0;
		});

		if(calc_md5) {
			const auto md5 = util::generate_md5(path + patch.file_meta.name);
			assert(md5.size() == patch.file_meta.md5.size());
			std::ranges::copy(md5, patch.file_meta.md5.data());
			dirty = true;
		}

		if(dirty) {
			dao.update(patch);
		}
	}

	return patches;
}

} // ember