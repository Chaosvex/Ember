/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <shared/utility/ConsoleColour.h>

#ifdef _WIN32
	#include <Windows.h>
#else
	#include <iostream>
	#include <string_view>
#endif

namespace ember::util {

namespace {

#if defined(_WIN32)

WORD colour_attribute(Colour colour) {
	WORD attribute = 0;
	
	switch(colour) {
		case Colour::BLACK:
			attribute = 0;
			break;
		case Colour::LIGHT_BLUE:
			attribute = FOREGROUND_INTENSITY;
			[[fallthrough]];
		case Colour::BLUE:
			attribute |= FOREGROUND_BLUE;
			break;
		case Colour::LIGHT_RED:
			attribute = FOREGROUND_INTENSITY;
			[[fallthrough]];
		case Colour::RED:
			attribute |= FOREGROUND_RED;
			break;
		case Colour::BROWN:
			attribute = FOREGROUND_GREEN | FOREGROUND_RED;
			break;
		case Colour::LIGHT_CYAN:
			attribute = FOREGROUND_INTENSITY;
			[[fallthrough]];
		case Colour::CYAN:
			attribute |= FOREGROUND_BLUE | FOREGROUND_GREEN;
			break;
		case Colour::LIGHT_GREEN:
			attribute = FOREGROUND_INTENSITY;
			[[fallthrough]];
		case Colour::GREEN:
			attribute |= FOREGROUND_GREEN;
			break;
		case Colour::LIGHT_MAGENTA:
			attribute = FOREGROUND_INTENSITY;
			[[fallthrough]];
		case Colour::MAGENTA:
			attribute |= FOREGROUND_BLUE | FOREGROUND_RED;
			break;
		case Colour::DARK_GREY:
			attribute = FOREGROUND_INTENSITY;
			break;
		case Colour::WHITE:
			attribute = FOREGROUND_INTENSITY;
			[[fallthrough]];
		case Colour::GREY:
			attribute |= FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
			break;
		case Colour::YELLOW:
			attribute = FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_RED;
			break;
		case Colour::DEFAULT:
			// shutting the compiler up
			break;
	}

	return attribute;
}

#else

std::string_view ansi_sequence(Colour colour) {
	switch(colour) {
		case Colour::BLACK:
			return "\033[22;30m";
		case Colour::BLUE:
			return "\033[22;34m";
		case Colour::BROWN:
			return "\033[22;33m";
		case Colour::CYAN:
			return "\033[22;36m";
		case Colour::DARK_GREY:
			return "\033[01;30m";
		case Colour::GREEN:
			return "\033[22;32m";
		case Colour::GREY:
			return "\033[22;37m";
		case Colour::LIGHT_BLUE:
			return "\033[01;34m";
		case Colour::LIGHT_CYAN:
			return "\033[01;36m";
		case Colour::LIGHT_GREEN:
			return "\033[01;32m";
		case Colour::LIGHT_MAGENTA:
			return "\033[01;35m";
		case Colour::LIGHT_RED:
			return "\033[01;31m";
		case Colour::MAGENTA:
			return "\033[22;35m";
		case Colour::RED:
			return "\033[22;31m";
		case Colour::WHITE:
			return "\033[01;37m";
		case Colour::YELLOW:
			return "\033[01;33m";
		default:
			return "\033[0m";
	}
}

#endif

} // unnamed

void set_output_colour(Colour colour) {
#if defined(_WIN32)
	HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(console, colour_attribute(colour));
#else
	std::cout << ansi_sequence(colour);
#endif
}

Colour save_output_colour() {
#if defined(_WIN32)
	const HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO buffer;
	GetConsoleScreenBufferInfo(stdout_handle, &buffer);
	return static_cast<Colour>(buffer.wAttributes);
#else
	return Colour::DEFAULT;
#endif
}

} // util, ember