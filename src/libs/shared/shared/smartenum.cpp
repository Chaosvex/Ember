/*
The MIT License (MIT)

Copyright (c) 2015 Tobias Widlund
Copyright (c) 2024 Ember

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "smartenum.hpp"
#include "utility/polyfill/print"
#include <format>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace smart_enum {

/*
 * Converts a single character to an integer value
 * If an error occurs, the return value is undefined 
 * and err is set.
 */
int char_to_int(char c, int base, bool& err) {
	err = false;

	if(c >= '0' && c <= '9') {
		const int val = c - '0';

		if(val >= base) {
			err = true;
		}

		return val;
	}

	c = ::tolower(c);

	if(c >= 'a' && c <= 'z') {
		const int val = c - 'a' + 10;

		if(val >= base) {
			err = true;
		}

		return val;
	}

	err = true;
	return 0;
}

auto svtoull(std::string_view string, int base) -> std::expected<unsigned long long, bool> {
	if(base == 0) {
		if(string.starts_with("0x") || string.starts_with("0X")) {
			base = 16;
			string = string.substr(2);
		} else if(string.starts_with("0b") || string.starts_with("0B")) {
			base = 2;
			string = string.substr(2);
		} else if(string.starts_with("0")) {
			base = 8;
			string = string.substr(1);
		} else {
			base = 10;
		}
	} else if(base == 2 || base == 16) {
		string = string.substr(2);
	} else if(base == 8) {
		string = string.substr(1);
	} 

	auto value = 0ull;

	for(const auto& byte : string) {
		bool err = false;
		auto digit = char_to_int(byte, base, err);
		value = (value * base) + (digit);

		if(err) {
			return std::unexpected(true);
		}
	}

	return value;
}

std::string_view trim_whitespace(std::string_view str) {
    // trim trailing whitespace
	const auto end_pos = str.find_last_not_of(" \t");

    if(end_pos != std::string_view::npos) {
        str = str.substr(0, end_pos + 1);
    }
    
    // trim leading spaces
    const auto start_pos = str.find_first_not_of(" \t");

    if(start_pos != std::string_view::npos) {
        str = str.substr(start_pos);
    }
    
    return str;
}
    
std::string_view extract_entry(std::string_view values) {
	const size_t next_comma_pos = values.find(',');
    
    if(next_comma_pos != std::string_view::npos) {
        std::string_view segment = values.substr(0, next_comma_pos);
        return trim_whitespace(segment);
    } else {
		return trim_whitespace(values);
    }
};

[[noreturn]] void print_and_bail(std::source_location location) {
	std::println(stderr, "enum parse failed! {}:#L{}", location.file_name(), location.line());
	std::exit(EXIT_FAILURE);
}

unsigned long long decimal_convert(std::string_view rhs, bool& error) {
	std::stringstream ss;
	ss << "0x" << std::hex << std::setw(2) << std::setfill('0');

	for(std::size_t i = 1; i < rhs.size() - 1; ++i) {
		ss << static_cast<unsigned>(rhs[i]);
	}

	const auto str = ss.view();
	auto result = svtoull(str); 

	if(!result) {
		error = true;
	}

	return *result;
}
    
} // smart_enum