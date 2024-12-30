/*
 * Copyright (c) 2016 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "PCREHelper.h"
#include <boost/algorithm/string.hpp>
#include <pcre.h>
#include <string>
#include <memory>

using namespace std::string_literals;

namespace ember::util::pcre {

Result utf8_jit_compile(std::string expression) {
	int error_offset = 0;
	const char* error = 0;

	// <\ isn't handled by PCRE (unless you happen to be WoW.exe), replace with \b
	boost::replace_first(expression, R"(\<)", R"(\b)");
	boost::replace_first(expression, R"(\>)", R"(\b)");

	auto compiled = std::unique_ptr<::pcre, void(*)(void*)>(
		pcre_compile(expression.c_str(), PCRE_UTF8 | PCRE_CASELESS, &error, &error_offset, nullptr),
		pcre_free
	);

	if(compiled == nullptr) {
		throw std::runtime_error("Could not compile expression, " + expression + ":" + error);
	}

	auto extra = std::unique_ptr<pcre_extra, void(*)(pcre_extra*)>(
		pcre_study(compiled.get(), PCRE_STUDY_JIT_COMPILE, &error),
		pcre_free_study
	);

	if(extra == nullptr) {
		throw std::runtime_error("Could not JIT compile expression, " + expression + ":" + error);
	}

	return { std::move(compiled), std::move(extra) };
}

int match(const std::string& needle, const Result& result) {
	return pcre_exec(result.pcre.get(), result.extra.get(), needle.c_str(), needle.size(), 0, 0, nullptr, 0);
}

} // pcre, util, ember