/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

namespace ember::util {

enum class Colour : unsigned int {
	BLACK, BLUE, GREEN, CYAN, RED, MAGENTA,
	BROWN, GREY, DARK_GREY, LIGHT_BLUE, LIGHT_GREEN,
	LIGHT_CYAN, LIGHT_RED, LIGHT_MAGENTA, YELLOW, WHITE,
	DEFAULT
};

void set_output_colour(Colour colour);
Colour save_output_colour();

class ConsoleColour final {
	Colour original_;

public:
	ConsoleColour() : original_(save_output_colour()) {}

	~ConsoleColour() {
		reset();
	}

	void set(Colour colour) {
		set_output_colour(colour);
	}

	void reset() {
		set_output_colour(original_);
	}
};

} // util, ember