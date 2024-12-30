/*
 * Copyright (c) 2014, 2016 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <cstdint>
#include <boost/endian/arithmetic.hpp>
#include <shared/utility/MulticharConstant.h>

namespace ember::dbc {

namespace be = boost::endian;

#pragma pack(push, 1)

constexpr std::uint32_t DBC_MAGIC = util::make_mcc("WDBC");

struct Header {
	be::big_uint32_t magic;
	be::little_uint32_t records;
	be::little_uint32_t fields;
	be::little_uint32_t record_size;
	be::little_uint32_t string_block_len;
};

#pragma pack(pop)

} // dbc, ember