/*
 * Copyright (c) 2018 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <spark/buffers/pmr/BufferWrite.h>
#include <spark/buffers/detail/SharedDefs.h>
#include <spark/Exception.h>
#include <cstddef>

namespace ember::spark::io::pmr {

class NullBuffer final : public BufferWrite {
public:
	using size_type       = std::size_t;
	using value_type      = std::byte;
	using contiguous      = is_contiguous;
	using seeking         = unsupported;

	void write(const auto& elem) {}
	void write(const void* source, size_type length) override {};
	void read(auto* elem) {}
	void read(void* destination, size_type length) {};
	void copy(auto* elem) const {}
	void copy(void* destination, size_type length) const {};
	void reserve(const size_type length) override {};
	size_type size() const override{ return 0; };
	[[nodiscard]] bool empty() const override { return true; };
	bool can_write_seek() const override { return false; }

	void write_seek(const BufferSeek direction, const std::size_t offset) override {
		throw exception("Don't do this on a NullBuffer"); 
	};
};

} // pmr, io, spark, ember