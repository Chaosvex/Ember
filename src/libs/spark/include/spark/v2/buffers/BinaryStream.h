/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <spark/buffers/Utility.h>
#include <spark/Exception.h>
#include <algorithm>
#include <string>
#include <cstddef>
#include <cstring>

namespace ember::spark::v2 {

template <typename buf_type>
concept writeable =
	requires(buf_type t, void* v, std::size_t s) {
		{ t.write(v, s) } -> std::same_as<void>;
};

enum class StreamState {
	OK, READ_LIMIT_ERR, BUFF_LIMIT_ERR
};

template<byte_oriented buf_type>
class BinaryStream final {
	buf_type& buffer_;
	std::size_t total_write_ = 0;
	std::size_t total_read_ = 0;
	const std::size_t read_limit_;
	StreamState state_;

	void check_read_bounds(const std::size_t read_size) {
		if(read_size > buffer_.size()) {
			state_ = StreamState::BUFF_LIMIT_ERR;
			throw buffer_underrun(read_size, total_read_, buffer_.size());
		}

		const auto req_total_read = total_read_ + read_size;

		if(read_limit_ && req_total_read > read_limit_) {
			state_ = StreamState::READ_LIMIT_ERR;
			throw stream_read_limit(read_size, total_read_, read_limit_);
		}

		total_read_ = req_total_read;
	}

public:
	explicit BinaryStream(buf_type& source, std::size_t read_limit = 0)
		: buffer_(source), read_limit_(0) {};

	/*** Write ***/

	BinaryStream& operator <<(const trivially_copyable auto& data) requires(writeable<buf_type>) {
		buffer_.write(&data, sizeof(data));
		total_write_ += sizeof(data);
		return *this;
	}

	BinaryStream& operator <<(const std::string& data) requires(writeable<buf_type>) {
		buffer_.write(data.data(), data.size());
		const char term = '\0';
		buffer_.write(&term, 1);
		total_write_ += (data.size() + 1);
		return *this;
	}

	BinaryStream& operator <<(const char* data) requires(writeable<buf_type>) {
		const auto len = std::strlen(data);
		buffer_.write(data, len);
		total_write_ += len;
		return *this;
	}

	template<typename T>
	void put(const T* data, std::size_t count) requires(writeable<buf_type>) {
		const auto write_size = count * sizeof(T);
		buffer_.write(data, write_size);
		total_write_ += write_size;
	}

	template<typename It>
	void put(It begin, const It end) requires(writeable<buf_type>) {
		for(auto it = begin; it != end; ++it) {
			*this << *it;
		}
	}

	/*** Read ***/

	// terminates when it hits a null-byte or consumes all data in the buffer
	BinaryStream& operator >>(std::string& dest) {
		char byte;

		do { // not overly efficient
			check_read_bounds(1);
			buffer_.read(&byte, 1);

			if(byte) {
				dest.push_back(byte);
			}
		} while(byte && buffer_.size() > 0);

		return *this;
	}

	BinaryStream& operator >>(trivially_copyable auto& data) {
		check_read_bounds(sizeof(data));
		buffer_.read(&data, sizeof(data));
		return *this;
	}

	void get(std::string& dest, std::size_t size) {
		check_read_bounds(size);
		dest.resize(size);
		buffer_.read(dest.data(), size);
	}

	template<typename T>
	void get(T* dest, std::size_t count) {
		const auto read_size = count * sizeof(T);
		check_read_bounds(read_size);
		buffer_.read(dest, read_size);
	}

	template<typename It>
	void get(It begin, const It end) {
		for(; begin != end; ++begin) {
			*this >> *begin;
		}
	}

	/**  Misc functions **/

	bool can_write_seek() const requires(writeable<buf_type>) {
		return buffer_.can_write_seek();
	}

	void write_seek(SeekDir direction, std::size_t offset = 0) requires(writeable<buf_type>) {
		buffer_.write_seek(direction, offset);
	}

	std::size_t size() const {
		return buffer_.size();
	}

	bool empty() {
		return buffer_.empty();
	}

	std::size_t total_write() {
		requires(writeable<buf_type>);
	return total_write_;
	}

	buf_type* buffer() {
		return &buffer_;
	}

	void skip(std::size_t count) {
		check_read_bounds(count);
		buffer_.skip(count);
	}

	StreamState state() {
		return state_;
	}

	std::size_t total_read() {
		return total_read_;
	}

	std::size_t read_limit() {
		return read_limit_;
	}
};

} // v2, spark, ember