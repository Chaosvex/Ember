/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/utility/MulticharConstant.h>
#include <cstdint>

namespace ember::mpq {

constexpr std::uint16_t HEADER_ALIGNMENT = 0x200;
constexpr std::uint32_t MPQA_FOURCC = util::make_mcc("MPQ\x1a");
constexpr std::uint32_t MPQB_FOURCC = util::make_mcc("MPQ\x1b");
constexpr std::uint32_t HEADER_SIZE_V0 = 0x20;
constexpr std::uint32_t HEADER_SIZE_V1 = 0x2C;
constexpr std::uint32_t HEADER_SIZE_V2 = 0x2C; // minimum
constexpr std::uint32_t HEADER_SIZE_V3 = 0xD0;
constexpr std::uint32_t MPQ_KEY_HASH_TABLE = 0xC3AF3770;
constexpr std::uint32_t MPQ_KEY_BLOCK_TABLE = 0xEC83B3A3;
constexpr std::uint32_t MPQ_HASH_ENTRY_EMPTY = 0xFFFFFFFF;
constexpr std::uint32_t MPQ_HASH_ENTRY_DELETED = 0xFFFFFFFE;
constexpr std::uint32_t MPQ_HASH_TABLE_INDEX = 0;
constexpr std::uint32_t MPQ_HASH_NAME_A = 1;
constexpr std::uint32_t MPQ_HASH_NAME_B = 2;
constexpr std::uint32_t MPQ_HASH_FILE_KEY = 3;
constexpr std::uint32_t BLOCK_SIZE = 0x200;
constexpr std::uint32_t SECTOR_SIZE_HINT = 0x200 << 3;

enum Compression {
	MPQ_COMPRESSION_HUFFMANN        = 0x01,
	MPQ_COMPRESSION_ZLIB            = 0x02,
	MPQ_COMPRESSION_PKWARE          = 0x08,
	MPQ_COMPRESSION_BZIP2           = 0x10,
	MPQ_COMPRESSION_SPARSE          = 0x20,
	MPQ_COMPRESSION_ADPCM_MONO      = 0x40,
	MPQ_COMPRESSION_ADPCM_STEREO    = 0x80,
	MPQ_COMPRESSION_LZMA            = 0x12,
	MPQ_COMPRESSION_NEXT_SAME       = 0xFFFFFFFF,

	MPQ_COMPRESSION_MASK = 0xFB
};

constexpr std::uint8_t LZMA_UNCOMPRESSED_SIZE = 8;

} // mpq, ember