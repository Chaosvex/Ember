/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <mpq/v0/MemoryArchive.h>
#include <boost/endian/conversion.hpp>
#include <mpq/Structures.h>
#include <mpq/SharedDefs.h>
#include <mpq/Crypt.h>
#include <shared/utility/polyfill/start_lifetime_as>
#include <bit>

namespace ember::mpq::v0 {

MemoryArchive::MemoryArchive(std::span<std::byte> buffer) : mpq::MemoryArchive(buffer) {
	block_table_ = fetch_block_table();
	hash_table_ = fetch_hash_table();
	decrypt_block(std::as_writable_bytes(block_table_), MPQ_KEY_BLOCK_TABLE);
	decrypt_block(std::as_writable_bytes(hash_table_), MPQ_KEY_HASH_TABLE);
	load_listfile(0);
}

void MemoryArchive::extract_file(const std::filesystem::path& path, ExtractionSink& store) {
	extract_file_ext(path, store, 0);
}

std::span<HashTableEntry> MemoryArchive::fetch_hash_table() const {
	auto entry = std::start_lifetime_as<HashTableEntry>(
		buffer_.data() + header_->hash_table_offset
	);

	return { entry, header_->hash_table_size };
}

std::span<BlockTableEntry> MemoryArchive::fetch_block_table() const {
	auto entry = std::start_lifetime_as<BlockTableEntry>(
		buffer_.data() + header_->block_table_offset
	);

	return { entry, header_->block_table_size };
}

const Header* MemoryArchive::header() const {
	return std::start_lifetime_as<Header>(buffer_.data());
}

std::size_t MemoryArchive::size() const {
	return boost::endian::little_to_native(header_->archive_size);
}

} // v0, mpq, ember