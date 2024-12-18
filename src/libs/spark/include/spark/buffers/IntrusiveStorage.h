/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once
#pragma warning(disable : 4996)

#include <spark/buffers/SharedDefs.h>
#include <array>
#include <concepts>
#include <type_traits>
#include <cassert>
#include <cstring>
#include <cstddef>

namespace ember::spark::io::detail {

struct IntrusiveNode {
	IntrusiveNode* next;
	IntrusiveNode* prev;
};

template<std::size_t BlockSize, byte_type StorageType = std::byte>
struct IntrusiveStorage final {
	using value_type = StorageType;
	using OffsetType = std::remove_const_t<decltype(BlockSize)>;

	OffsetType read_offset = 0;
	OffsetType write_offset = 0;
	IntrusiveNode node {};
	std::array<value_type, BlockSize> storage;

	void reset() {
		read_offset = 0;
		write_offset = 0;
	}

	std::size_t write(const auto source, std::size_t length) {
		assert(!region_overlap(source, length, storage.data(), storage.size()));
		std::size_t write_len = BlockSize - write_offset;

		if(write_len > length) {
			write_len = length;
		}

		std::memcpy(storage.data() + write_offset, source, write_len);
		write_offset += static_cast<OffsetType>(write_len);
		return write_len;
	}

	std::size_t copy(auto destination, const std::size_t length) const {
		assert(!region_overlap(storage.data(), storage.size(), destination, length));
		std::size_t read_len = BlockSize - read_offset;

		if(read_len > length) {
			read_len = length;
		}

		std::memcpy(destination, storage.data() + read_offset, read_len);
		return read_len;
	}

	std::size_t read(auto destination, const std::size_t length, const bool allow_optimise = false) {
		std::size_t read_len = copy(destination, length);
		read_offset += static_cast<OffsetType>(read_len);

		if(read_offset == write_offset && allow_optimise) {
			reset();
		}

		return read_len;
	}

	std::size_t skip(const std::size_t length, const bool allow_optimise = false) {
		std::size_t skip_len = BlockSize - read_offset;

		if(skip_len > length) {
			skip_len = length;
		}

		read_offset += static_cast<OffsetType>(skip_len);

		if(read_offset == write_offset && allow_optimise) {
			reset();
		}

		return skip_len;
	}

	std::size_t size() const {
		return write_offset - read_offset;
	}

	std::size_t free() const {
		return BlockSize - write_offset;
	}

	void write_seek(const BufferSeek direction, const std::size_t offset) {
		switch(direction) {
			case BufferSeek::SK_ABSOLUTE:
				write_offset = offset;
				break;
			case BufferSeek::SK_BACKWARD:
				write_offset -= static_cast<OffsetType>(offset);
				break;
			case BufferSeek::SK_FORWARD:
				write_offset += static_cast<OffsetType>(offset);
				break;
		}
	}

	std::size_t advance_write(std::size_t size) {
		const auto remaining = free();

		if(remaining < size) {
			size = remaining;
		}

		write_offset += static_cast<OffsetType>(size);
		return size;
	}

	const value_type* read_data() const {
		return storage.data() + read_offset;
	}

	value_type* write_data() {
		return storage.data() + write_offset;
	}

	value_type& operator[](const std::size_t index) {
		return *(storage.data() + index);
	}

	value_type& operator[](const std::size_t index) const {
		return *(storage.data() + index);
	}
};

} // detail, io, spark, ember