/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Utility.h"
#include <filesystem>
#include <cctype>

namespace ember {

/*
 * Removes a FlatBuffers reflected namespace, leaving us with
 * just the name of the type. For example:
 * ember.spark.MessageType -> MessageType
 */
std::string remove_fbs_ns(std::string_view name) {
	auto loc = name.find_last_of('.');

	if(loc == std::string_view::npos) {
		return std::string(name);
	}

	return std::string(name.substr(loc + 1));
}

/*
 * Takes a FlatBuffers reflected input schema file and returns
 *  the name sans extension. For example:
 * //InputSchema.fbs -> InputSchema
 */
std::string fbs_to_name(std::string_view name) {
	std::string_view fixed {name};

	while(fixed.starts_with('/')) {
		fixed = fixed.substr(1, name.size());
	}

	std::filesystem::path p(fixed);
	return p.replace_extension().string();
}

/*
 * Takes a reflected FlatBuffers namespace and converts it to one that
 * can be used in C++. For example:
 * ember.spark.v2 -> ember::spark
 */
std::string to_cpp_ns(std::string_view val) {
	std::string result {val};

	for(auto it = result.begin(); it != result.end();) {
		if(*it == '.') {
			*it = ':';
			it = result.insert(it, ':');
		} else {
			++it;
		}
	}

	return result;
}

/*
 * Converts from Pascal or camel case to snake case
 */
std::string snake_case(std::string_view val) {
	std::string result;
	bool first = true;

	for(auto ch : val) {
		if(std::isupper(ch)) { 
			if(!first) {
				result = result + '_';
			}

			result += std::tolower(ch);
		} else { 
			result = result + ch; 
		}

		first = false;
	} 

	return result; 
}

} // ember