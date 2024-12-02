/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <logger/Sink.h>
#include <logger/FileWrapper.h>
#include <logger/Utility.h>
#include <shared/util/cstring_view.hpp>
#include <boost/container/small_vector.hpp>
#include <string>
#include <utility>
#include <vector>
#include <cstdio>
#include <cstdint>

namespace ember::log {

class FileSink final : public Sink {
	static constexpr auto SV_RESERVE = 256u;
	static constexpr auto MAX_BUF_SIZE = 4096u;

public:
	enum class Mode { TRUNCATE, APPEND };

private:
	File file_;
	std::string file_name_;
	std::string file_name_format_;
	std::uintmax_t  max_size_ = 0;
	std::uintmax_t current_size_ = 0;
	unsigned int rotations_ = 0;
	bool log_severity_ = true;
	bool log_date_ = false;
	bool midnight_rotate_ = false;
	int last_mday_ = detail::current_time().tm_mday;
	cstring_view time_format_ = "[%d/%m/%Y %H:%M:%S] ";
	boost::container::small_vector<char, SV_RESERVE> out_buf_;

	void open(Mode mode = Mode::TRUNCATE);
	void rotate();
	void rotate_check(std::size_t buffer_size, const std::tm& curr_time);
	void format_file_name();
	bool file_exists(const std::string& name);
	void set_initial_rotation();
	std::string generate_record_detail(Severity severity, const std::tm& curr_time);

public:
	FileSink(Severity severity, Filter filter, std::string file_name, Mode mode);
	~FileSink();

	void log_severity(bool enable) { log_severity_ = enable; }
	void log_date(bool enable) { log_date_ = enable;  }
	void midnight_rotate(bool enable) { midnight_rotate_ = enable; }
	void size_limit(std::uintmax_t megabytes);
	void time_format(cstring_view format);
	void write(Severity severity, Filter type, std::span<const char> record, bool flush) override;
	void batch_write(const std::span<std::pair<RecordDetail, std::vector<char>>>& records) override;
};


} // log, ember