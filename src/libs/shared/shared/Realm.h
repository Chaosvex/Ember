/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <dbcreader/MemoryDefs.h>
#include <shared/utility/enum_bitmask.h>
#include <string>
#include <cstdint>

namespace ember {

struct Realm {
	enum class CreationSetting : std::uint8_t {
		ENABLED,
		DISABLED,
		QUEUED,
		EXISTING_ONLY
	};

	enum class Flags : std::uint8_t {
		NONE         = 0x00,
		INVALID      = 0x01,
		OFFLINE      = 0x02,
		SPECIFYBUILD = 0x04,
		UNK1         = 0x08,
		UNK2         = 0x10,
		RECOMMENDED  = 0x20, // can set manually or allow client to do so by setting the population to 600.0f
		NEW_PLAYERS  = 0x40, // can set manually or allow client to do so by setting the population to 200.0f
		FULL         = 0x80  // can set manually or allow client to do so by setting the population to 400.0f
	}; 

	enum class Type : std::uint32_t {
		PvE, PvP, RP = 6, RPPvP = 8,
	};

	std::uint32_t id;
	std::string name;
	std::string ip;
	std::uint16_t port;
	std::string address;
	float population;
	Type type;
	Flags flags;
	dbc::Cfg_Categories::Category category;
	dbc::Cfg_Categories::Region region;
	CreationSetting creation_setting;
};

ENABLE_BITMASK(Realm::Flags);

} // ember