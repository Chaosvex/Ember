/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Survey.h"
#include <shared/utility/FileMD5.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cassert>
#include <stdexcept>

namespace ember {

void Survey::add_data(const grunt::Platform platform, const grunt::System os, const std::string& path) {
	std::ifstream file(path, std::ios::binary);

	if(!file) {
		throw std::runtime_error("Error opening " + path);
	}

	FileMeta fmeta {
		.name = "Survey",
		.size = std::filesystem::file_size(path)
	};

	std::vector<std::byte> buffer(static_cast<std::size_t>(fmeta.size));
	file.read(reinterpret_cast<char*>(buffer.data()), fmeta.size);

	if(!file.good()) {
		throw std::runtime_error("An error occured while reading " + path);
	}

	const auto md5 = util::generate_md5(buffer);
	static_assert(md5.size() == fmeta.md5.size());
	std::ranges::copy(md5, fmeta.md5.data());

	const Key key {
		.platform = platform,
		.os = os,
	};

	meta_.insert_or_assign(key, std::move(fmeta));
	data_.insert_or_assign(key, std::move(buffer));
}

std::optional<std::reference_wrapper<const FileMeta>>
Survey::meta(const grunt::Platform platform, const grunt::System os) const {
	if(auto it = meta_.find({ platform, os }); it != meta_.end()) {
		return std::ref(it->second);
	}

	return std::nullopt;
}

std::optional<std::span<const std::byte>> Survey::data(const grunt::Platform platform,
                                                       const grunt::System os) const {
	if(auto it = data_.find({ platform, os }); it != data_.end()) {
		return it->second;
	}

	return std::nullopt;
}

std::uint32_t Survey::id() const {
	return id_;
}

} // ember