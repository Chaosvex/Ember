/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <exception>
#include <string>

namespace ember {

class exception : public std::exception {
	std::string msg_;

public:
	exception(std::string msg) : msg_(std::move(msg)) { };

	virtual const char* what() const noexcept override {
		return msg_.c_str();
	}
};

} // ember