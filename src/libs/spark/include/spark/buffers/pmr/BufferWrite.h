/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <spark/buffers/pmr/BufferBase.h>
#include <spark/buffers/detail/SharedDefs.h>
#include <cstddef>

namespace ember::spark::io::pmr {

class BufferWrite : virtual public BufferBase {
public:
	using value_type = std::byte;

	virtual ~BufferWrite() = default;
	virtual void write(const void* source, std::size_t length) = 0;
	virtual void reserve(std::size_t length) = 0;
	virtual bool can_write_seek() const = 0;
	virtual void write_seek(BufferSeek direction, std::size_t offset) = 0;
};

} // pmr, io, spark, ember