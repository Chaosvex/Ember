/*
 * Copyright (c) 2016 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <memory>
#include <string>
#include <pcre.h>
#include <cstddef>

namespace ember::util::pcre {

struct Result {
	std::unique_ptr<::pcre, void(*)(void*)> pcre;
	std::unique_ptr<::pcre_extra, void(*)(pcre_extra*)> extra;
};

Result utf8_jit_compile(std::string expression);
int match(const std::string& needle, const Result& result);

} // utf8, util, ember