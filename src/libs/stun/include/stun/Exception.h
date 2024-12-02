/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <stun/Logging.h>
#include <stdexcept>
#include <string>
#include <cstddef>

namespace ember::stun {

class exception : public std::runtime_error {
	Error value;

public:
	exception(Error value)
		: value(value), std::runtime_error("An unknown STUN exception occured!") { }
	exception(Error value, const std::string& msg)
		: value(value), std::runtime_error(msg) { };
};

class parse_error final : public exception {
public:
	parse_error(Error value)
		: exception(value, "An unknown STUN parser exception occured!") { }
	parse_error(Error value, const std::string& msg)
		: exception(value, msg) { };
};

} // stun, ember