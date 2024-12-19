/*
* Copyright (c) 2024 Ember
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <spark/buffers/pmr/BufferWrite.h>
#include <spark/buffers/detail/SharedDefs.h>
#include <vector>
#include <utility>
#include <cassert>
#include <cstddef>
#include <cstring>

namespace ember::spark::io::pmr {

template<byte_oriented buf_type>
class BufferWriteAdaptor : public BufferWrite {
	buf_type& buffer_;
	std::size_t write_;

public:
	BufferWriteAdaptor(buf_type& buffer) : buffer_(buffer), write_(buffer.size()) {}

	void write(auto& source) {
		write(&source, sizeof(source));
	}

	void write(const void* source, std::size_t length) override {
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

		std::memcpy(buffer_.data() + write_, source, length);
		write_ += length;
	}

	void reserve(const std::size_t length) override {
		buffer_.reserve(length);
	}

	bool can_write_seek() const override {
		return true;
	}

	void write_seek(const BufferSeek direction, const std::size_t offset) override {
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

	auto underlying_data() const {
		return buffer_.data();
	}

	auto write_ptr() {
		return buffer_.data() + write_;
	}

	const auto write_ptr() const {
		return buffer_.data() + write_;
	}
	
	void reset() {
		write_ = 0;
		buffer_.clear();
	}
};

} // pmr, io, spark, ember