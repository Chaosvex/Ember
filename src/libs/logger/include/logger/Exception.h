/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <stdexcept>
#include <string>
 
namespace ember::log {

class exception : public std::runtime_error {
public:
	exception() : std::runtime_error("An unknown logging exception occured!") { }
	exception(const std::string& msg) : std::runtime_error(msg) { };
};

} // log, ember