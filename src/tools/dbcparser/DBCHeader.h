/*
 * Copyright (c) 2019 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/util/MulticharConstant.h>
#include <boost/endian/arithmetic.hpp>
#include <string_view>
#include <stdexcept>
#include <sstream>
#include <cstdint>
#include <cstddef>

constexpr std::uint32_t DBC_MAGIC = ember::util::make_mcc("WDBC");
#define DBC_HEADER_SIZE 20

namespace be = boost::endian;

namespace ember::dbc {

struct Header {
	be::big_uint32_t magic;
	be::little_uint32_t records;
	be::little_uint32_t fields;
	be::little_uint32_t record_size;
	be::little_uint32_t string_block_size;
};

inline void validate_dbc(std::string_view name, const Header& header, const std::size_t expect_size,
                         const std::size_t expect_fields, const std::size_t dbc_size) {
	if(header.magic != DBC_MAGIC) {
		std::stringstream err;
		err << name << ": " << "Invalid header magic - found 0x" << std::hex << header.magic
		    << ", expected 0x" << DBC_MAGIC;
		throw std::runtime_error(err.str());
	}

	if(header.record_size != expect_size || header.fields != expect_fields) {
		std::stringstream err;
		err << name << ": " << "Expected " << expect_fields << " fields, " << expect_size << " byte records "
		    << "but DBC has " << header.fields << " fields and " << header.record_size << " byte records";
		throw std::runtime_error(err.str());
	}

	const std::size_t calculated_size =
		sizeof(Header) + header.string_block_size + (header.record_size * header.records);

	if(calculated_size != dbc_size) {
		std::stringstream err;
		err << name << ": " << "Invalid size! Expected " << calculated_size << " bytes but the file was "
		    << dbc_size << " bytes";
		throw std::runtime_error(err.str());
	}
}

} // dbc, ember