/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <array>
#include <string_view>

namespace ember {

inline const std::array<std::string_view, 5> winx86 {
	"WoW.exe", "fmod.dll", "ijl15.dll",
    "dbghelp.dll", "unicows.dll"
};

inline const std::array<std::string_view, 5> macx86 {
	"MacOS/World of Warcraft", "Info.plist",
    "Resources/Main.nib/objects.xib",
	"Resources/wow.icns", "PkgInfo"
};

inline const std::array<std::string_view, 5> macppc {
	"MacOS/World of Warcraft", "Info.plist",
    "Resources/Main.nib/objects.xib",
    "Resources/wow.icns", "PkgInfo" 
};

} // ember