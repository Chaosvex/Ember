/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <mpq/v1/MemoryArchive.h>
#include <boost/endian/conversion.hpp>
#include <mpq/Structures.h>
#include <mpq/SharedDefs.h>
#include <mpq/Exception.h>
#include <mpq/Crypt.h>
#include <shared/utility/polyfill/start_lifetime_as>

namespace ember::mpq::v1 {

MemoryArchive::MemoryArchive(std::span<std::byte> buffer) : mpq::MemoryArchive(buffer) {
	header_ = std::start_lifetime_as<v1::Header>(buffer.data());
	validate();

	block_table_ = fetch_block_table();
	hash_table_ = fetch_hash_table();

	if(header_->extended_block_table_offset) {
		bt_hi_pos_ = fetch_btable_hi_pos();
	}

	decrypt_block(std::as_writable_bytes(block_table_), MPQ_KEY_BLOCK_TABLE);
	decrypt_block(std::as_writable_bytes(hash_table_), MPQ_KEY_HASH_TABLE);
	
	auto index = file_lookup("(listfile)", 0);

	if(!index) {
		return;
	}

	auto hi_mask= 0;

	if(bt_hi_pos_) {
		hi_mask = high_mask((*bt_hi_pos_)[index]);
	}

	load_listfile(hi_mask);
}

void MemoryArchive::validate() {
	if(header_->extended_block_table_offset > buffer_.size_bytes()) {
		throw exception("open error: bad ext block table offset");
	}

	auto btable_pos = extend(header_->block_table_offset_hi, header_->block_table_offset);
	auto htable_pos = extend(header_->hash_table_offset_hi, header_->hash_table_offset);

	if(btable_pos > buffer_.size_bytes()) {
		throw exception("open error: block table out of bounds");
	}

	if(htable_pos > buffer_.size_bytes()) {
		throw exception("open error: hash table out of bounds");
	}

	auto btable_end = btable_pos + (static_cast<unsigned long long>(header_->block_table_size) * sizeof(BlockTableEntry));
	auto htable_end = htable_pos + (static_cast<unsigned long long>(header_->hash_table_size) * sizeof(HashTableEntry));

	if(btable_end < btable_pos) {
		throw exception("open error: block table too large");
	}

	if(htable_end < htable_pos) {
		throw exception("open error: hash table too large");
	}

	if(btable_end > buffer_.size_bytes()) {
		throw exception("open error: block table entries out of bounds");
	}

	if(htable_end > buffer_.size_bytes()) {
		throw exception("open error: hash table entries out of bounds");
	}
}

std::uint64_t MemoryArchive::extend(std::uint16_t hi, std::uint32_t lo) const {
	return (static_cast<std::uint64_t>(hi) << 32) | lo;
}

std::uint64_t MemoryArchive::high_mask(std::uint16_t value) const {
	return static_cast<std::uint64_t>(value) << 32;
}

std::span<std::uint16_t> MemoryArchive::fetch_btable_hi_pos() const {
	const auto bt_hi_pos = std::start_lifetime_as<std::uint16_t>(
		buffer_.data() + header_->extended_block_table_offset
	);

	return { bt_hi_pos, header_->block_table_size };
}

void MemoryArchive::extract_file(const std::filesystem::path& path, ExtractionSink& store) {
	auto index = file_lookup(path.string(), 0);

	if(index == npos) {
		throw exception("cannot extract file: file not found");
	}

	auto fpos_hi = 0;

	if(bt_hi_pos_) {
		fpos_hi = high_mask((*bt_hi_pos_)[index]);;
	}

	extract_file_ext(path, store, fpos_hi);
}

std::span<HashTableEntry> MemoryArchive::fetch_hash_table() const {
	auto entry = std::start_lifetime_as<HashTableEntry>(
		buffer_.data() + extend(header_->hash_table_offset_hi, header_->hash_table_offset)
	);

	return { entry, header_->hash_table_size };
}

std::span<BlockTableEntry> MemoryArchive::fetch_block_table() const {
	auto entry = std::start_lifetime_as<BlockTableEntry>(
		buffer_.data() + extend(header_->block_table_offset_hi, header_->block_table_offset)
	);

	return { entry, header_->block_table_size };
}

const Header* MemoryArchive::header() const {
	return std::start_lifetime_as<const Header>(buffer_.data());
}

std::size_t MemoryArchive::size() const {
	const uintptr_t bt_end = 
		extend(header_->block_table_offset_hi, header_->block_table_offset)
		+ (static_cast<unsigned long long>(header_->block_table_size) * sizeof(BlockTableEntry));

	const std::uintptr_t ht_end = 
		extend(header_->hash_table_offset_hi, header_->hash_table_offset)
		+ (static_cast<unsigned long long>(header_->hash_table_size) * sizeof(HashTableEntry));

	const uintptr_t ebt_end = header_->extended_block_table_offset
		+ (static_cast<unsigned long long>(header_->block_table_size) * sizeof(BlockTableEntry));

	const auto size = ht_end > bt_end? ht_end : bt_end;
	return ebt_end > size? ebt_end : size;
}

} // v1, mpq, ember