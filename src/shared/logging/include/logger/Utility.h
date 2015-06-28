/*
 * Copyright (c) 2015 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <logger/Severity.h>
#include <string>
#include <ctime>

namespace ember { namespace log { 

Severity severity_string(const std::string& severity);

namespace detail {

std::string severity_string(Severity severity);
std::tm current_time();
std::string put_time(const std::tm& time, const std::string& format);

}}} //detail, log, ember