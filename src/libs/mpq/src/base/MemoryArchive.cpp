/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <mpq/base/MemoryArchive.h>
#include <mpq/Compression.h>
#include <mpq/Crypt.h>
#include <mpq/Exception.h>
#include <mpq/MemorySink.h>
#include <mpq/MPQ.h>
#include <mpq/Structures.h>
#include <shared/utility/polyfill/start_lifetime_as>
#include <boost/endian/conversion.hpp>
#include <boost/container/small_vector.hpp>
#include <bit>
#include <iterator>
#include <cmath>

namespace ember::mpq {

MemoryArchive::MemoryArchive(std::span<std::byte> buffer)
	: buffer_(buffer),
	  header_(std::start_lifetime_as<v0::Header>(buffer_.data())) {
	validate();
}

void MemoryArchive::load_listfile(const std::uint64_t fpos_hi) {
	auto index = file_lookup("(listfile)", 0);

	if(index == npos) {
		return;
	}
	
	const auto& entry = file_entry(index);

	std::string buffer;

	buffer.resize_and_overwrite(entry.uncompressed_size, [&](char* strbuf, std::size_t size) {
		MemorySink sink(std::as_writable_bytes(std::span(strbuf, size)));
		extract_file_ext("(listfile)", sink, fpos_hi);
		return size;
	});

	parse_listfile(std::move(buffer));
}

void MemoryArchive::parse_listfile(std::string buffer) {
	std::stringstream stream;
	stream.str(std::move(buffer));
	std::string line;

	while(std::getline(stream, line, '\n')) {
		std::erase(line, '\r');
		files_.emplace_back(std::move(line));
		line.clear(); // reset to known state
	}
}

int MemoryArchive::version() const { 
	return boost::endian::little_to_native(header_->format_version);
}

std::size_t MemoryArchive::file_lookup(std::string_view name, const std::uint16_t locale) const {
	const auto table = hash_table();
	auto index = hash_string(name, MPQ_HASH_TABLE_INDEX);

	if(!table.empty()) {
		index %= table.size();
	}
	
	if(table[index].block_index == MPQ_HASH_ENTRY_EMPTY) {
		return npos;
	}

	const auto name_key_a = hash_string(name, MPQ_HASH_NAME_A);
	const auto name_key_b = hash_string(name, MPQ_HASH_NAME_B);
	const auto end = (index - 1) > table.size()? table.size() : index - 1;

	for(auto i = index; i != end; ++i, i %= table.size()) {
		if(table[i].block_index == MPQ_HASH_ENTRY_EMPTY) {
			return npos;
		}

		if(table[i].name_1 == name_key_a && table[i].name_2 == name_key_b
		   && table[i].locale == locale) {
			return i;
		}
	}

	return npos;
}

std::span<std::uint32_t> MemoryArchive::file_sectors(const BlockTableEntry& entry) {
	const auto sector_size = BLOCK_SIZE << header_->block_size_shift;
	auto count = static_cast<std::uint32_t>(
		std::ceil(entry.uncompressed_size / static_cast<double>(sector_size)) + 1
	);

	if(entry.flags & Flags::MPQ_FILE_SECTOR_CRC) {
		++count;
	}

	auto sector_begin = std::start_lifetime_as<std::uint32_t>(buffer_.data() + entry.file_position);
	return { sector_begin, count };
}

void MemoryArchive::extract_file_ext(const std::filesystem::path& path,
                                     ExtractionSink& store,
                                     const std::uint64_t fpos_hi) {
	auto index = file_lookup(path.string(), 0);

	if(index == npos) {
		throw exception("cannot extract file: file not found");
	}

	const auto filename = path.filename().string();
	auto& entry = file_entry(index);
	auto key = hash_string(filename, MPQ_HASH_FILE_KEY);

	if(entry.flags & MPQ_FILE_FIX_KEY) {
		key = (key + entry.file_position) ^ entry.uncompressed_size;
		entry.flags = static_cast<Flags>(entry.flags & ~Flags::MPQ_FILE_FIX_KEY);
	}

	if(buffer_.size_bytes() < (entry.file_position + fpos_hi) + entry.compressed_size) {
		throw exception("cannot extract file: file out of bounds");
	}

	if(entry.flags & MPQ_FILE_SINGLE_UNIT) {
		extract_single_unit(entry, key, fpos_hi, store);
	} else if(entry.flags & MPQ_FILE_COMPRESS_MASK) {
		extract_compressed(entry, key, fpos_hi, store);
	} else {
		extract_uncompressed(entry, key, fpos_hi, store);
	}

	entry.flags = static_cast<Flags>(entry.flags & ~Flags::MPQ_FILE_ENCRYPTED);
}

void MemoryArchive::extract_compressed(BlockTableEntry& entry,
                                       std::uint32_t key,
                                       const std::uint64_t fpos_hi,
                                       ExtractionSink& store) {
	auto max_sector_size = BLOCK_SIZE << header_->block_size_shift;
	const auto file_offset = buffer_.data() + (entry.file_position | fpos_hi);

	auto sectors = file_sectors(entry);

	// decrypt the sector block if required
	if(entry.flags & Flags::MPQ_FILE_ENCRYPTED) {
		decrypt_block(std::as_writable_bytes(sectors), --key);
	}

	std::size_t ignore_count = 1;

	if(entry.flags & Flags::MPQ_FILE_SECTOR_CRC) {
		++ignore_count;
	}

	sectors = std::span(sectors.begin(), sectors.end() - ignore_count);
	auto remaining = entry.uncompressed_size;

	boost::container::small_vector<std::byte, SECTOR_SIZE_HINT> buffer(
		max_sector_size, boost::container::default_init
	);

	const int def_comp = default_compression(sectors, file_offset, entry.uncompressed_size);

	for(auto sector = sectors.begin(); sector != sectors.end(); ++sector) {
		std::uint32_t sector_size_actual = max_sector_size;
		std::uint32_t sector_size = max_sector_size;

		// if we're at the end of the file and need to read less
		if(max_sector_size > remaining) {
			sector_size = remaining;
			sector_size_actual = remaining;
		}
		
		// sector is compressed, get the actual data size
		if(entry.flags & Flags::MPQ_FILE_COMPRESS_MASK) {
			sector_size_actual =  *std::next(sector) - *sector;
		}

		// get the location of the data for this sector
		std::span sector_data(
			std::start_lifetime_as<std::byte>(file_offset + *sector), sector_size_actual
		);

		if(entry.flags & Flags::MPQ_FILE_ENCRYPTED) {
			decrypt_block(sector_data, ++key);
		}

		if(sector_size_actual < sector_size) {
			extract_compressed_sector(sector_data, buffer, entry.flags, store, def_comp);
		} else {
			store(sector_data);
		}

		remaining -= sector_size;
	}
}

void MemoryArchive::extract_compressed_sector(std::span<const std::byte> input,
                                              std::span<std::byte> output,
											  const Flags flags,
                                              ExtractionSink& store,
                                              const int def_comp) {
	if(flags & Flags::MPQ_FILE_COMPRESS) {
		auto ret = decompress(input, output, def_comp);

		if(!ret) {
			throw exception("cannot extract file: decompression failed");
		}

		store({ output.data(), *ret });
	} else if(flags & Flags::MPQ_FILE_IMPLODE) {
		auto ret = decompress_pklib(input, output);

		if(!ret) {
			throw exception("cannot extract file: decompression (explode) failed");
		}

		store({ output.data(), *ret });
	} else {
		throw exception("cannot extract file: unknown compression flag");
	}
}

void MemoryArchive::extract_uncompressed(BlockTableEntry& entry,
                                         const std::uint32_t key,
										 const std::uint64_t fpos_hi,
                                         ExtractionSink& store) {
	const std::span data(
		buffer_.data() + (entry.file_position | fpos_hi), entry.uncompressed_size
	);

	if(entry.flags & Flags::MPQ_FILE_ENCRYPTED) {
		decrypt_block(data, key);
	}

	store(data);
}

void MemoryArchive::extract_single_unit(BlockTableEntry& entry, const std::uint32_t key,
                                        const std::uint64_t fpos_hi, ExtractionSink& store) {
	const std::span data(
		buffer_.data() + (entry.file_position | fpos_hi), entry.uncompressed_size
	);

	if(entry.flags & Flags::MPQ_FILE_ENCRYPTED) {
		decrypt_block(data, key);
	}

	if(!(entry.flags & Flags::MPQ_FILE_COMPRESS_MASK)) {
		store(data);
		return;
	}

	boost::container::small_vector<std::byte, SECTOR_SIZE_HINT> buffer(
		entry.uncompressed_size, boost::container::default_init
	);

	if(entry.uncompressed_size != entry.compressed_size) {
		extract_compressed_sector(data, buffer, entry.flags, store);
	} else {
		store(data);
	}
}

int MemoryArchive::default_compression(std::span<const std::uint32_t> sectors,
                                       const std::byte* const offset,
                                       const std::uint32_t size) const {
	if(sectors.empty() || !size) {
		return -1;
	}

	auto data = offset + sectors.front();
	auto comp_mask = std::bit_cast<std::uint8_t>(data[0]);
	const auto initial = next_compression(comp_mask);

	if(!initial) {
		return -1;
	}

	return initial;
}

BlockTableEntry& MemoryArchive::file_entry(const std::size_t index) {
	const auto block_index = hash_table_[index].block_index;
	return block_table_[block_index];
}

const BlockTableEntry& MemoryArchive::file_entry(const std::size_t index) const {
	const auto block_index = hash_table_[index].block_index;
	return block_table_[block_index];
}

std::span<const BlockTableEntry> MemoryArchive::block_table() const {
	return block_table_;
}

std::span<const HashTableEntry> MemoryArchive::hash_table() const {
	return hash_table_;
}

std::span<const std::string> MemoryArchive::files() const {
	return files_;
}

void MemoryArchive::files(std::span<std::string_view> files) {
	for(auto& file : files) {
		files_.emplace_back(file);
	}
}

void MemoryArchive::validate() {
	if(!validate_header(*header_)) {
		throw exception("open error: unexpected header size");
	}

	if(buffer_.size_bytes() < header_->header_size) {
		throw exception("open error: unexpected end of header");
	}

	if(header_->archive_size) {
		if(buffer_.size_bytes() < header_->archive_size) {
			throw exception("open error: unexpected end of archive");
		}
	}

	if(header_->block_table_offset + header_->block_table_size
	   < header_->block_table_offset) {
		throw exception("open error: block table too big");
	}

	if(header_->hash_table_offset + header_->hash_table_size
	   < header_->hash_table_offset) {
		throw exception("open error: hash table too big");
	}

	const auto bt_end = header_->block_table_offset
		+ (header_->block_table_size * sizeof(BlockTableEntry));
	const auto ht_end = header_->hash_table_offset
		+ (header_->hash_table_size * sizeof(HashTableEntry));

	if(bt_end < header_->block_table_offset) {
		throw exception("open error: block table too big");
	}

	if(ht_end < header_->hash_table_offset) {
		throw exception("open error: hash table too big");
	}

	if(buffer_.size_bytes() < bt_end) {
		throw exception("open error: block table out of bounds");
	}

	if(buffer_.size_bytes() < ht_end) {
		throw exception("open error: hash table out of bounds");
	}
}

} // mpq, ember