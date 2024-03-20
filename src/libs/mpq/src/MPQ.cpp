/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <mpq/MPQ.h>
#include <mpq/Exception.h>
#include <mpq/Structures.h>
#include <mpq/Archive.h>
#include <spark/v2/buffers/BinaryStream.h>
#include <spark/v2/buffers/BufferAdaptor.h>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/endian/arithmetic.hpp>
#include <boost/endian/buffers.hpp>
#include <array>
#include <bit>
#include <fstream>
#include <cstddef>
#include <cstdio>

using namespace boost::interprocess;

namespace ember::mpq {

LocateResult archive_offset(FILE* file) {
	std::array<std::uint32_t, 1> magic{};
	auto buffer = std::as_writable_bytes(std::span(magic));
	auto ret = std::fseek(file, 0, SEEK_END);

	if(ret != 0) {
		return std::unexpected(ErrorCode::FILE_READ_FAILED);
	}

	const auto size = std::ftell(file);

	if(size == -1) {
		return std::unexpected(ErrorCode::FILE_READ_FAILED);
	}

	for(std::uintptr_t i = 0; i < (size - buffer.size_bytes()); i += HEADER_ALIGNMENT) {
		ret = std::fseek(file, i, SEEK_SET);

		if(ret != 0) {
			return std::unexpected(ErrorCode::FILE_READ_FAILED);
		}

		ret = std::fread(buffer.data(), buffer.size_bytes(), 1, file);

		if(!ret) {
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
	spark::v2::BufferAdaptor adaptor(buffer);
	spark::v2::BinaryStream stream(adaptor);
	std::uint32_t magic{};

	for(std::uintptr_t i = 0; i < buffer.size() - sizeof(magic); i += HEADER_ALIGNMENT) {
		stream >> magic;
		boost::endian::big_to_native_inplace(magic);

		if(magic == MPQA_FOURCC) {
			return i;
		}

		if(stream.size() > HEADER_ALIGNMENT) {
			stream.skip(HEADER_ALIGNMENT - sizeof(magic));
		}
	}

	return npos;
}

LocateResult locate_archive(const std::filesystem::path& path) try {
	if(!std::filesystem::exists(path)) {
		return std::unexpected(ErrorCode::FILE_NOT_FOUND);
	}

	FILE* file = std::fopen(path.string().c_str(), "rb");

	if(!file) {
		return std::unexpected(ErrorCode::UNABLE_TO_OPEN);
	}

	const auto offset = archive_offset(file);

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

std::unique_ptr<Archive> open_archive(const std::filesystem::path& path,
									  const std::uintptr_t offset,
									  const bool map) {
	std::error_code ec{};

	if(!std::filesystem::exists(path, ec) || ec) {
		throw exception("cannot open archive: file not found");
	}

	std::ifstream stream;
	stream.open(path, std::ios::binary | std::ios::in);

	if(!stream.is_open()) {
		throw exception("cannot open archive: stream is_open failed");
	}

	std::array<char, sizeof(v0::Header)> h_buf{};
	stream.seekg(offset, std::ios::beg);
	stream.read(h_buf.data(), h_buf.size());

	if(!stream.good()) {
		throw exception("cannot read archive: bad seek offset");
	}

	stream.close();

	const auto header_v0 = std::bit_cast<const v0::Header*>(h_buf.data());

	if(!validate_header(*header_v0)) {
		throw exception("cannot open archive: bad header encountered");
	}

	if(header_v0->format_version == 0) {
		if(map) {
			file_mapping file(path.c_str(), read_write);
			mapped_region region(file, copy_on_write);
			region.advise(mapped_region::advice_sequential);
			return std::make_unique<v0::MappedArchive>(std::move(file), std::move(region));
		} else {
			return std::make_unique<v0::FileArchive>(path, offset);
		}
	}

	return nullptr;
}

std::unique_ptr<MemoryArchive> open_archive(std::span<std::byte> data,
                                            const std::uintptr_t offset) {
	const auto header_v0 = std::bit_cast<const v0::Header*>(data.data() + offset);

	if(header_v0->format_version == 0) {
		const auto adjusted = std::span(data.data() + offset, data.size_bytes() - offset);
		return std::make_unique<v0::MemoryArchive>(adjusted);
	}

	return nullptr;
}

std::unique_ptr<mpq::FileArchive> create_archive(const std::uint32_t version) {
	return nullptr;
}

// todo
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