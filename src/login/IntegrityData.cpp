/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "IntegrityData.h"
#include <shared/util/FNVHash.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <cstddef>

namespace fs = std::filesystem;

namespace ember {

IntegrityData::IntegrityData(std::span<const GameVersion> versions, std::string_view path) {
	std::array<std::string_view, 5> winx86 { "WoW.exe", "fmod.dll", "ijl15.dll",
	                                         "dbghelp.dll", "unicows.dll" };

	std::array<std::string_view, 5> macx86 { "MacOS/World of Warcraft", "Info.plist",
	                                          "Resources/Main.nib/objects.xib",
	                                          "Resources/wow.icns", "PkgInfo" };

	std::array<std::string_view, 5> macppc { "MacOS/World of Warcraft", "Info.plist",
	                                         "Resources/Main.nib/objects.xib",
	                                         "Resources/wow.icns", "PkgInfo" };
	
	for(auto& version : versions) {
		load_binaries(path, version.build, winx86, grunt::System::Win, grunt::Platform::x86);
		load_binaries(path, version.build, macx86, grunt::System::OSX, grunt::Platform::x86);
		load_binaries(path, version.build, macppc, grunt::System::OSX, grunt::Platform::PPC);
	}

	// ensure we have at least one supported client
	if(data_.empty()) {
		throw std::runtime_error("Client integrity checking is enabled but no binaries were found");
	}
}

std::optional<std::span<const std::byte>>
IntegrityData::lookup(const GameVersion version, const grunt::Platform platform,
                      const grunt::System os) const {
	auto it = data_.find(hash(version.build, platform, os));

	if(it == data_.end()) {
		return std::nullopt;
	}
	
	return it->second;
}

void IntegrityData::load_binaries(std::string_view path, std::uint16_t build,
                                  std::span<std::string_view> files,
                                  const grunt::System system,
                                  const grunt::Platform platform) {
	auto full_path = std::format("{}{}_{}_{}", path, grunt::to_string(system),
		grunt::to_string(platform), std::to_string(build));

	std::transform(full_path.begin(), full_path.end(), full_path.begin(),
				   [](unsigned char c){ return std::tolower(c); });

	fs::path dir(full_path);

	if(!fs::is_directory(dir)) {
		return;
	}

	auto fnv = hash(build, platform, system);
	std::vector<std::byte> buffer;
	std::size_t write_offset = 0;

	// write all of the binary data into a single buffer
	for(auto& f : files) {
		fs::path fpath = dir;
		fpath /= f.data();

		std::ifstream file(fpath.string(), std::ios::binary | std::ios::ate);

		if(!file) {
			throw std::runtime_error("Unable to open " + fpath.string());
		}

		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);
		buffer.resize(static_cast<std::size_t>(size) + buffer.size());

		if(!file.read(reinterpret_cast<char*>(buffer.data()) + write_offset, size)) {
			throw std::runtime_error("Unable to read " + fpath.string());
		}

		write_offset += static_cast<std::size_t>(size);
	}

	data_.emplace(fnv, std::move(buffer));
}

std::size_t IntegrityData::hash(std::uint16_t build, grunt::Platform platform,
                                grunt::System os) const {
	FNVHash hasher;
	hasher.update(build);
	hasher.update(platform);
	return hasher.update(os);
}

} // ember
