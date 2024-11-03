/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "IntegrityData.h"
#include "IntegrityPlatforms.h"
#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <cstddef>

namespace fs = std::filesystem;

namespace ember {

void IntegrityData::add_version(const GameVersion& version, std::string_view path) {	
	load_binaries(path, version.build, winx86, grunt::System::Win, grunt::Platform::x86);
	load_binaries(path, version.build, macx86, grunt::System::OSX, grunt::Platform::x86);
	load_binaries(path, version.build, macppc, grunt::System::OSX, grunt::Platform::PPC);

	// ensure we have at least one supported client for the given version
	if(data_.empty()) {
		throw std::runtime_error("Client integrity checking is enabled but no binaries were found");
	}
}

void IntegrityData::add_versions(std::span<const GameVersion> versions, std::string_view path) {
	for(auto& version : versions) {
		add_version(version, path);
	}
}

auto IntegrityData::lookup(const GameVersion version,
                           const grunt::Platform platform,
                           const grunt::System os) const  -> std::optional<std::span<const std::byte>> {
	if(auto it = data_.find({ version.build, platform, os }); it != data_.end()) {
		return it->second;
	} else {
		return std::nullopt;
	}
}

void IntegrityData::load_binaries(std::string_view path, std::uint16_t build,
                                  std::span<const std::string_view> files,
                                  const grunt::System system,
                                  const grunt::Platform platform) {
	auto full_path = std::format("{}{}_{}_{}", path, grunt::to_string(system),
		grunt::to_string(platform), std::to_string(build));

	std::ranges::transform(full_path, full_path.begin(), [](const unsigned char c){
		return std::tolower(c); 
	});

	fs::path dir(full_path);

	if(!fs::is_directory(dir)) {
		return;
	}

	std::vector<std::byte> buffer;
	std::uintmax_t write_offset = 0;

	// write all of the binary data into a single buffer
	for(const auto& f : files) {
		fs::path fpath = dir;
		fpath /= f.data();

		std::ifstream file(fpath, std::ios::binary);

		if(!file) {
			throw std::runtime_error("Unable to open " + fpath.string());
		}

		const auto size = fs::file_size(fpath);
		buffer.resize(static_cast<std::size_t>(size) + buffer.size());

		if(!file.read(reinterpret_cast<char*>(buffer.data()) + write_offset, size)) {
			throw std::runtime_error("Unable to read " + fpath.string());
		}

		write_offset += size;
	}

	data_.emplace(detail::Key{ build, platform, system }, std::move(buffer));
}

} // ember