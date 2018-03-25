/*
 * Copyright (c) 2018 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Sink.h"
#include <string>

namespace ember {

class ConsoleSink final : public Sink {
	std::string time_fmt_ = "%Y-%m-%dT%H:%M:%SZ"; // ISO 8601, can be overriden by header

	void print_opcode(const fblog::Message& message);

public:
	void handle(const fblog::Header& header);
	void handle(const fblog::Message& message);
};

} // ember