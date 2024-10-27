/*
 * Copyright (c) 2021 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <spark/buffers/pmr/StreamBase.h>
#include <spark/buffers/pmr/BufferWrite.h>
#include <shared/util/cstring_view.hpp>
#include <algorithm>
#include <array>
#include <concepts>
#include <string>
#include <string_view>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ember::spark::io::pmr {

class BinaryStreamWriter : virtual public StreamBase {
private:
	BufferWrite& buffer_;
	std::size_t total_write_;

	template<std::size_t size>
	constexpr auto generate_filled(const std::uint8_t value) {
		std::array<std::uint8_t, size> target{};
		std::ranges::fill(target, value);
		return target;
	}

public:
	explicit BinaryStreamWriter(BufferWrite& source)
		: StreamBase(source), buffer_(source), total_write_(0) {}

	BinaryStreamWriter& operator <<(const is_pod auto& data) {
		buffer_.write(&data, sizeof(data));
		total_write_ += sizeof(data);
		return *this;
	}

	BinaryStreamWriter& operator <<(const std::string& data) {
		buffer_.write(data.data(), data.size() + 1); // +1 also writes terminator
		total_write_ += (data.size() + 1);
		return *this;
	}

	BinaryStreamWriter& operator <<(const char* data) {
		assert(data);
		const auto len = std::strlen(data);
		buffer_.write(data, len + 1); // include terminator
		total_write_ += len + 1;
		return *this;
	}

	BinaryStreamWriter& operator <<(std::string_view& data) {
		buffer_.write(data.data(), data.size());
		const char term = '\0';
		buffer_.write(&term, sizeof(term));
		total_write_ += (data.size() + 1);
		return *this;
	}

	BinaryStreamWriter& operator <<(cstring_view& data) {
		buffer_.write(data.data(), data.size() + 1);
		total_write_ += (data.size() + 1);
		return *this;
	}

	template<std::ranges::contiguous_range range>
	void put(range& data) {
		const auto write_size = data.size() * sizeof(range::value_type);
		buffer_.write(data.data(), write_size);
		total_write_ += write_size;
	}

	template<typename T>
	void put(const T& data) requires(std::integral<T> || std::floating_point<T>) {
		buffer_.write(&data, sizeof(T));
		total_write_ += sizeof(T);
	}

	template<typename T>
	void put(const T* data, std::size_t count) {
		assert(data);
		const auto write_size = count * sizeof(T);
		buffer_.write(data, write_size);
		total_write_ += write_size;
	}

	template<typename It>
	void put(It begin, const It end) {
		for(auto it = begin; it != end; ++it) {
			*this << *it;
		}
	}

	template<std::size_t size>
	void fill(const std::uint8_t value) {
		const auto filled = generate_filled<size>(value);
		buffer_.write(filled.data(), filled.size());
		total_write_ += size;
	}

	/**  Misc functions **/ 

	bool can_write_seek() const {
		return buffer_.can_write_seek();
	}

	void write_seek(const StreamSeek direction, const std::size_t offset) {
		if(direction == StreamSeek::SK_STREAM_ABSOLUTE) {
			buffer_.write_seek(BufferSeek::SK_BACKWARD, total_write_ - offset);
		} else {
			buffer_.write_seek(static_cast<BufferSeek>(direction), offset);
		}
	}

	std::size_t size() const {
		return buffer_.size();
	}

	[[nodiscard]]
	bool empty() const {
		return buffer_.empty();
	}

	std::size_t total_write() const {
		return total_write_;
	}

	BufferWrite* buffer() const {
		return &buffer_;
	}
};

} // pmr, io, spark, ember
