/*
 * Copyright (c) 2019 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once 

#include <cstdint>

namespace ember::util {

constexpr std::uint32_t make_mcc(char const (&s)[4]) {
    return s[0] << 16 | s[1] << 8 | s[2];
}

constexpr std::uint32_t make_mcc(char const (&s)[5]) {
    return s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3];
}

} // util, ember