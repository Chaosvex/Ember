/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <logger/Severity.h>
#include <shared/utility/cstring_view.hpp>
#include <string>
#include <string_view>
#include <ctime>

namespace ember::log { 

Severity severity_string(std::string_view severity);

namespace detail {

std::string_view severity_string(Severity severity);
std::tm current_time();
std::string put_time(const std::tm& time, cstring_view format);

}} // detail, log, ember