/*
 * Copyright (c) 2016 - 2019 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/utility/enum_bitmask.h>
#include <shared/utility/UTF8String.h>
#include <string>
#include <cstdint>

namespace ember {

struct Vector {
	float x, y, z;
};

struct CharacterTemplate { // used during creation
	utf8_string name;
	std::uint8_t race;
	std::uint8_t class_;
	std::uint8_t gender;
	std::uint8_t skin;
	std::uint8_t face;
	std::uint8_t hairstyle;
	std::uint8_t haircolour;
	std::uint8_t facialhair;
	std::uint8_t outfit_id;
};

struct Character { // used for character list display
	enum class Flags : std::uint32_t { // todo, investigate
		NONE                   = 0x000,
		UNKNOWN1               = 0x001,
		UNKNOWN2               = 0x002,
		LOCKED_FOR_TRANSFER    = 0x004,
		UNKNOWN4               = 0x008,
		UNKNOWN5               = 0x010,
		UNKNOWN6               = 0x020,
		UNKNOWN7               = 0x040,
		UNKNOWN8               = 0x080,
		UNKNOWN9               = 0x100,
		UNKNOWN10              = 0x200,
		HIDE_HELM              = 0x400,
		HIDE_CLOAK             = 0x800,
		UNKNOWN13              = 0x1000,
		GHOST                  = 0x2000,
		RENAME                 = 0x4000
	};

	utf8_string name;
	utf8_string internal_name;
	std::uint64_t id;
	std::uint32_t account_id;
	std::uint32_t realm_id;
	std::uint8_t race;
	std::uint8_t class_;
	std::uint8_t gender;
	std::uint8_t skin;
	std::uint8_t face;
	std::uint8_t hairstyle;
	std::uint8_t haircolour;
	std::uint8_t facialhair;
	std::uint8_t level;
	std::uint32_t zone;
	std::uint32_t map;
	std::uint32_t guild_id;
	std::uint32_t guild_rank;
	Vector position;
	float orientation;
	Flags flags;
	bool first_login;
	std::uint32_t pet_display;
	std::uint32_t pet_level;
	std::uint32_t pet_family;
};

ENABLE_BITMASK(Character::Flags);

} // ember