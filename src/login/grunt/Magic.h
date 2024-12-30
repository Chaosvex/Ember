/*
 * Copyright (c) 2015 - 2019 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/utility/MulticharConstant.h>
#include <shared/smartenum.hpp>
#include <cstdint>

/* This file isn't as exciting as the name implies */

namespace ember::grunt {

smart_enum_class(Game, std::uint32_t,
	WoW = util::make_mcc("WoW")
);

smart_enum_class(Platform, std::uint32_t,
	x86 = util::make_mcc("x86"),
	PPC = util::make_mcc("PPC")
);

smart_enum_class(System, std::uint32_t,
	Win = util::make_mcc("Win"),
	OSX = util::make_mcc("OSX")
);

smart_enum_class(Locale, std::uint32_t,
	enGB = util::make_mcc("enGB"), enUS = util::make_mcc("enUS"),
	esMX = util::make_mcc("esMX"), ptBR = util::make_mcc("ptBR"),
	frFR = util::make_mcc("frFR"), deDE = util::make_mcc("deDE"),
	esES = util::make_mcc("esES"), ptPT = util::make_mcc("ptPT"),
	itIT = util::make_mcc("itIT"), ruRU = util::make_mcc("ruRU"),
	koKR = util::make_mcc("koKR"), zhTW = util::make_mcc("zhTW"),
	enTW = util::make_mcc("enTW"), enCN = util::make_mcc("enCN")
);

} // grunt, ember
