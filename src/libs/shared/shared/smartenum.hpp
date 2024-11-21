/*
The MIT License (MIT)

Copyright (c) 2015 Tobias Widlund

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

#pragma once

#include <logger/Logger.h>
#include <algorithm>
#include <ostream>
#include <expected>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace smart_enum {

int char_to_int(char c, int base, bool& err);
std::expected<unsigned long long, bool> svtoull(std::string_view string, int base = 0);
std::string_view trim_whitespace(std::string_view str);
std::string_view extract_entry(std::string_view values);
unsigned long long decimal_convert(std::string_view rhs, bool& conv_err);
[[noreturn]] void print_and_bail(std::source_location location);

static std::string_view unknown_enum_str { "UNKNOWN_ENUM_VALUE" };

template<typename SizeType>
std::unordered_map<SizeType, std::string_view> make_enum_map(std::source_location location,
                                                             std::string_view enum_values_string) {
    std::unordered_map<SizeType, std::string_view> name_map;
    SizeType current_enum_value = 0;

    while(!enum_values_string.empty()) {
        std::string_view current_enum_entry = extract_entry(enum_values_string);
		std::size_t next_comma_pos = enum_values_string.find(',');

		if(next_comma_pos != std::string_view::npos) {
			enum_values_string = enum_values_string.substr(next_comma_pos + 1);
		} else {
			enum_values_string = std::string_view{};
		}

        std::size_t equal_sign_pos = current_enum_entry.find('=');

        if(equal_sign_pos != std::string_view::npos) {
			std::string_view rhs = trim_whitespace(current_enum_entry.substr(equal_sign_pos + 1));
                
            // Handle using util::mcc_char - this makes me sad too
            if(auto first = rhs.find_first_of('"'), last = rhs.find_last_of('"');
                first != std::string_view::npos && first != last) {
				rhs = rhs.substr(first, (last - first) + 1);
            }

			const auto opening = rhs[0];
			const auto closing = rhs[rhs.size() - 1];
			bool parse_error = false;

			if((opening == '\'' && closing == '\'') || (opening == '"' && closing == '"')) {
				current_enum_value = static_cast<SizeType>(decimal_convert(rhs, parse_error));
			} else {
				const auto result = svtoull(rhs);

				if(result) {
					current_enum_value = static_cast<SizeType>(*result);
				} else {
					parse_error = true;
				}
			}

			if(parse_error) {
				print_and_bail(location);
			}

			current_enum_entry = current_enum_entry.substr(0, equal_sign_pos - 1);
        }
    
		current_enum_entry = trim_whitespace(current_enum_entry);
		name_map.insert_or_assign(current_enum_value, current_enum_entry);
        ++current_enum_value;
    }

    return name_map;
}

} // smart_enum

#define smart_enum(Type, Underlying, ...) enum Type : Underlying { __VA_ARGS__}; \
    static std::unordered_map<Underlying, std::string_view> Type##_enum_names \
		= smart_enum::make_enum_map<Underlying>(std::source_location::current(), #__VA_ARGS__); \
    \
    inline std::string_view Type##_to_string(Type value) try { \
        return Type##_enum_names.at((Underlying)value); \
    } catch(std::out_of_range&) { \
		return smart_enum::unknown_enum_str; \
	} \
    \

#define smart_enum_class(Type, Underlying, ...) enum class Type : Underlying { __VA_ARGS__}; \
    static std::unordered_map<Underlying, std::string_view> Type##_enum_names \
		= smart_enum::make_enum_map<Underlying>(std::source_location::current(), #__VA_ARGS__); \
    \
    inline std::string_view to_string(Type value) try { \
        return Type##_enum_names.at((Underlying)value);\
    } catch(std::out_of_range&) { \
		return smart_enum::unknown_enum_str; \
	} \
    \
    inline std::ostream& operator<<(std::ostream& outStream, Type value) { \
        outStream << to_string(value); \
        return outStream; \
    } \
    inline log::Logger& operator <<(log::Logger& outStream, Type value) { \
        outStream << to_string(value); \
        return outStream; \
    }

#define CREATE_FORMATTER(name) \
	template<> \
	struct std::formatter<name, char> { \
		template<typename ParseContext> \
		constexpr ParseContext::iterator parse(ParseContext& ctx) { \
			return ctx.begin(); \
		} \
	\
		template<typename FmtContext> \
		FmtContext::iterator format(name value, FmtContext& ctx) const { \
			const auto& str = to_string(value); \
			return std::ranges::copy(str.begin(), str.end(), ctx.out()).out; \
		} \
	};
