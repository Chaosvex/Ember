/*
 * Copyright (c) 2018 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <spark/buffers/SharedDefs.h>
#include <algorithm>
#include <concepts>
#include <ranges>
#include <utility>
#include <cassert>
#include <cstddef>
#include <cstring>

namespace ember::spark::io {

template <typename T>
concept can_resize = 
	requires(T t) {
		{ t.resize( std::size_t() ) } -> std::same_as<void>;
};

template<byte_oriented buf_type, bool space_optimise = true>
requires std::ranges::contiguous_range<buf_type>
class BufferAdaptor final {
public:
	using value_type = typename buf_type::value_type;
	using size_type  = typename buf_type::size_type;
	using contiguous = is_contiguous;
	using seeking    = supported;

	static constexpr size_type npos = -1;

private:
	buf_type& buffer_;
	size_type read_;
	size_type write_;

public:
	BufferAdaptor(buf_type& buffer) :
		buffer_(buffer),
		read_(0),
		write_(buffer.size()) {}

	template<typename T>
	void read(T* destination) {
		read(destination, sizeof(T));
	}

	void read(void* destination, size_type length) {
		assert(destination);
		copy(destination, length);
		read_ += length;

		if constexpr(space_optimise) {
			if(read_ == write_) {
				read_ = write_ = 0;
			}
		}
	}

	template<typename T>
	void copy(T* destination) const {
		copy(destination, sizeof(T));
	}

	void copy(void* destination, size_type length) const {
		assert(!region_overlap(buffer_.data(), buffer_.size(), destination, length));
		assert(destination);
		std::memcpy(destination, read_ptr(), length);
	}

	void skip(size_type length) {
		read_ += length;

		if constexpr(space_optimise) {
			if(read_ == write_) {
				read_ = write_ = 0;
			}
		}
	}

	void write(const auto& source) requires(can_resize<buf_type>) {
		write(source, sizeof(source));
	}

	void write(const void* source, size_type length) requires(can_resize<buf_type>) {
		assert(source && !region_overlap(source, length, buffer_.data(), buffer_.size()));
		const auto min_req_size = write_ + length;

		// we don't use std::back_inserter so we can support seeks
		if(buffer_.size() < min_req_size) {
			if constexpr(can_resize_overwrite<buf_type>) {
				buffer_.resize_and_overwrite(min_req_size, [](char*, std::size_t size) {
					return size;
				});
			} else {
				buffer_.resize(min_req_size);
			}
		}

		std::memcpy(write_ptr(), source, length);
		write_ += length;
	}

	size_type find_first_of(value_type val) const {
		const auto data = read_ptr();

		for(std::size_t i = 0u; i < size(); ++i) {
			if(data[i] == val) {
				return i;
			}
		}

		return npos;
	}
	
	size_type size() const {
		return buffer_.size() - read_;
	}

	[[nodiscard]]
	bool empty() const {
		return read_ == write_;
	}

	value_type& operator[](const size_type index) {
		return read_ptr()[index];
	}

	const value_type& operator[](const size_type index) const {
		return read_ptr()[index];
	}

	consteval static bool can_write_seek() requires(can_resize<buf_type>) {
		return std::is_same<seeking, supported>::value;
	}

	void write_seek(const BufferSeek direction, const size_type offset) requires(can_resize<buf_type>) {
		switch(direction) {
			case BufferSeek::SK_BACKWARD:
				write_ -= offset;
				break;
			case BufferSeek::SK_FORWARD:
				write_ += offset;
				break;
			case BufferSeek::SK_ABSOLUTE:
				write_ = offset;
		}
	}

	const auto read_ptr() const {
		return buffer_.data() + read_;
	}

	auto read_ptr() {
		return buffer_.data() + read_;
	}

	const auto write_ptr() const {
		return buffer_.data() + write_;
	}

	auto write_ptr() {
		return buffer_.data() + write_;
	}

	const auto data() const {
		return buffer_.data() + read_;
	}

	auto data() {
		return buffer_.data() + read_;
	}

	const auto storage() const {
		return buffer_.data();
	}

	auto storage() {
		return buffer_.data();
	}

	void advance_write(size_type bytes) {
		assert(buffer_.size() >= (write_ + bytes));
		write_ += bytes;
	}
};

} // io, spark, ember