/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <mpq/MPQ.h>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/endian/conversion.hpp>
#include <shared/util/polyfill/start_lifetime_as>
#include <array>
#include <bit>
#include <fstream>
#include <cstddef>
#include <cstring>

using namespace boost::interprocess;

namespace ember::mpq {

LocateResult archive_offset(std::ifstream& stream, const std::uintmax_t size) {
	std::array<std::uint32_t, 1> magic{};
	auto buffer = std::as_writable_bytes(std::span(magic));

	for(std::uintptr_t i = 0; i < (size - buffer.size_bytes()); i += HEADER_ALIGNMENT) {
		stream.seekg(i);

		if(!stream) {
			return std::unexpected(ErrorCode::FILE_READ_FAILED);
		}

		stream.read(reinterpret_cast<char*>(buffer.data()), buffer.size_bytes());

		if(!stream) {
			return std::unexpected(ErrorCode::FILE_READ_FAILED);
		}

		boost::endian::big_to_native_inplace(magic[0]);

		if(magic[0] == MPQA_FOURCC) {
			return i;
		}
	}

	return npos;
}

std::uintptr_t archive_offset(std::span<const std::byte> buffer) {
	std::uint32_t magic{};

	for(std::uintptr_t i = 0; i < buffer.size() - sizeof(magic); i += HEADER_ALIGNMENT) {
		std::memcpy(&magic, &buffer[i], sizeof(magic));
		boost::endian::big_to_native_inplace(magic);

		if(magic == MPQA_FOURCC) {
			return i;
		}
	}

	return npos;
}

LocateResult locate_archive(const std::filesystem::path& path) try {
	if(!std::filesystem::exists(path)) {
		return std::unexpected(ErrorCode::FILE_NOT_FOUND);
	}

	std::error_code ec;
	const auto size = std::filesystem::file_size(path, ec);

	if(ec) {
		return std::unexpected(ErrorCode::UNABLE_TO_OPEN);
	}

	std::ifstream stream(path, std::ios::binary | std::ios::in);
	
	if(!stream.is_open()) {
		return std::unexpected(ErrorCode::UNABLE_TO_OPEN);
	}

	const auto offset = archive_offset(stream, size);

	if(offset == npos) {
		return std::unexpected(ErrorCode::NO_ARCHIVE_FOUND);
	}

	return offset;
} catch(std::exception&) {
	return std::unexpected(ErrorCode::UNABLE_TO_OPEN);
}

LocateResult locate_archive(std::span<const std::byte> buffer) {
	const auto address = std::bit_cast<std::uintptr_t>(buffer.data());

	if(address % alignof(v0::Header)) {
		return std::unexpected(ErrorCode::BAD_ALIGNMENT);
	}

	const auto offset = archive_offset(buffer);

	if(offset == npos) {
		return std::unexpected(ErrorCode::NO_ARCHIVE_FOUND);
	}

	return offset;
}

std::unique_ptr<MemoryArchive> open_archive(const std::filesystem::path& path,
                                            const std::uintptr_t offset) {
	std::error_code ec{};

	if(!std::filesystem::exists(path, ec) || ec) {
		throw exception("cannot open archive: file not found");
	}

	std::ifstream stream(path, std::ios::binary | std::ios::in);

	if(!stream.is_open()) {
		throw exception("cannot open archive: stream is_open failed");
	}

	std::array<char, sizeof(v0::Header)> h_buf{};
	stream.seekg(offset);

	if(!stream.good()) {
		throw exception("cannot read archive: bad seek offset");
	}

	stream.read(h_buf.data(), h_buf.size());

	if(!stream) {
		throw exception("cannot read archive: header read failed");
	}

	stream.close();

	const auto header_v0 = std::start_lifetime_as<const v0::Header>(h_buf.data());

	if(!validate_header(*header_v0)) {
		throw exception("cannot open archive: bad header encountered");
	}

	file_mapping file(path.c_str(), read_only);
	mapped_region region(file, copy_on_write, offset);
	region.advise(mapped_region::advice_sequential);

	switch(header_v0->format_version) {
		case 0:
			return std::make_unique<v0::MappedArchive>(std::move(file), std::move(region));
		case 1:
			return std::make_unique<v1::MappedArchive>(std::move(file), std::move(region));
	}

	return nullptr;
}

std::unique_ptr<MemoryArchive> open_archive(std::span<std::byte> data,
                                            const std::uintptr_t offset) {
	const auto header_v0 = std::start_lifetime_as<const v0::Header>(data.data() + offset);
	const auto adjusted = std::span(data.data() + offset, data.size_bytes() - offset);

	if(header_v0->format_version == 0) {
		return std::make_unique<v0::MemoryArchive>(adjusted);
	} else if(header_v0->format_version == 1) {
		return std::make_unique<v1::MemoryArchive>(adjusted);
	}

	return nullptr;
}

bool validate_header(const v0::Header& header) {
	if(boost::endian::native_to_big(header.magic) != MPQA_FOURCC) {
		return false;
	}

	if(header.format_version == 0) {
		if(header.header_size != HEADER_SIZE_V0) {
			return false;
		}
	} else if(header.format_version == 1) {
		if(header.header_size != HEADER_SIZE_V1) {
			return false;
		}
	} else if(header.format_version == 2) {
		if(header.header_size >= HEADER_SIZE_V2) {
			return false;
		}
	} else if(header.format_version == 3) {
		if(header.header_size != HEADER_SIZE_V3) {
			return false;
		}
	} else {
		return false;
	}

	return true;
}

} // mpq, ember