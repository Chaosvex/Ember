/*
 * Copyright (c) 2016 - 2022 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "UTF8String.h"
#include <locale>
#include <string>
#include <cstddef>

namespace ember::util::utf8 {

utf8_string name_format(const utf8_string& string, const std::locale& locale);
bool is_alpha(const utf8_string& string, const std::locale& locale);
std::size_t max_consecutive(const utf8_string& string, bool case_insensitive = false, const std::locale& locale = std::locale());
std::size_t length(const utf8_string& utf8_string);
bool is_valid(const utf8_string& utf8_string);
bool is_valid(const char* utf8_string, std::size_t byte_length);

} // utf8, util, ember