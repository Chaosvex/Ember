/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <expected>
#include <string>
#include <thread>

namespace ember::thread {

enum Result {
	ok, unsupported
};

void set_affinity(std::thread& thread, unsigned int core);
void set_affinity(std::jthread& thread, unsigned int core);

Result set_name(const char* ascii_name);
Result set_name(std::thread& thread, const char* ascii_name);
Result set_name(std::jthread& thread, const char* ascii_name);

std::expected<std::wstring, Result> get_name(std::thread& thread);
std::expected<std::wstring, Result> get_name(std::jthread& thread);
std::expected<std::wstring, Result> get_name();

} // thread, ember
