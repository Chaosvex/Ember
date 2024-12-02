/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/util/cstring_view.hpp>
#include <cstdio>

#pragma warning(push)
#pragma warning(disable: 4996)

namespace ember::log {

class File final {
	std::FILE* file_ = nullptr;

public:
	File(cstring_view path, cstring_view mode) {
		file_ = std::fopen(path.c_str(), mode.c_str());
	}

	~File() {
		if(file_) {
			std::fclose(file_); 
		} 
	};
	
	operator FILE*() const {
		return file_; 
	}
	
	int close() {
		if(!file_) {
			return EOF;
		}

		const int ret = std::fclose(file_);
		file_ = nullptr;
		return ret;
	};

	std::FILE* handle() {
		return file_;
	}

	File() = default;

	File(const File&) = delete;
	File& operator=(const File&) = delete;

	File(File&& rhs) noexcept {
		file_ = rhs.file_;
		rhs.file_ = nullptr;
	}

	File& operator=(File&& rhs) noexcept {
		file_ = rhs.file_;
		rhs.file_ = nullptr;
		return *this;
	}
};

} // log, ember

#pragma warning(pop)