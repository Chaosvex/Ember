/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <logger/ConsoleSink.h>
#include <logger/Exception.h>
#include <logger/Utility.h>
#include <shared/util/ConsoleColour.h>
#include <algorithm>
#include <iterator>
#include <string>
#include <cstdio>
#include <cstring>

namespace ember::log {

void ConsoleSink::batch_write(const std::span<std::pair<RecordDetail, std::vector<char>>>& records) {
	if(!colour_) [[unlikely]] {
		do_batch_write(records);
	} else { // we can't do batch output if we need to colour each individual log record
		for(auto& [detail, data] : records) {
			write(detail.severity, detail.type, data, false);
		}
	}
}

void ConsoleSink::do_batch_write(const std::span<std::pair<RecordDetail, std::vector<char>>>& records) {
	std::size_t size = 0;
	Severity sink_sev = this->severity();
	Filter sink_filter = this->filter();
	bool matches = false;

	for(auto&& [detail, data] : records) {
		if(sink_sev <= detail.severity && !(sink_filter &detail.type)) {
			size += data.size();
			matches = true;
		}
	}

	if(!matches) {
		return;
	}

	out_buf_.reserve(size + (10 * records.size()));

	const bool prefix = !prefix_.empty();

	for(auto&& [detail, data] : records) {
		if(sink_sev <= detail.severity && !(sink_filter & detail.type)) {
			std::string_view severity = detail::severity_string(detail.severity);
			const auto cur_sz = out_buf_.size();
			const auto new_sz = cur_sz + severity.size() + data.size() + prefix_.size();
			out_buf_.resize(new_sz, boost::container::default_init);
			auto write_ptr = out_buf_.data() + cur_sz;
			std::size_t offset = 0;
			
			if(prefix) {
				std::memcpy(write_ptr + offset, prefix_.data(), prefix_.size());
				offset += prefix_.size();
			}
			
			std::memcpy(write_ptr + offset, severity.data(), severity.size());
			offset += severity.size();
			std::memcpy(write_ptr + offset, data.data(), data.size());
		}
	}

	std::fwrite(out_buf_.data(), out_buf_.size(), 1, stdout);
	out_buf_.clear();

	if(out_buf_.capacity() > MAX_BUF_SIZE) [[unlikely]] {
		out_buf_.shrink_to_fit();
	}
}

void ConsoleSink::write(Severity severity, Filter type, std::span<const char> record, bool flush) {
	if(this->severity() > severity || (this->filter() & type)) {
		return;
	}

	std::string_view sevsv = detail::severity_string(severity);

	out_buf_.clear();
	out_buf_.resize(prefix_.size() + record.size() + sevsv.size(), boost::container::default_init);

	std::size_t offset = 0;
	std::memcpy(out_buf_.data(), prefix_.data(), prefix_.size());
	offset += prefix_.size();
	std::memcpy(out_buf_.data() + offset, sevsv.data(), sevsv.size());
	offset += sevsv.size();
	std::memcpy(out_buf_.data() + offset, record.data(), record.size());


	if(colour_) [[likely]] {
		util::Colour old_colour;
		std::lock_guard guard(colour_lock);
		old_colour = util::save_output_colour();
		set_colour(severity);
		std::fwrite(out_buf_.data(), out_buf_.size(), 1, stdout);
		util::set_output_colour(old_colour);
	} else {
		std::fwrite(out_buf_.data(), out_buf_.size(), 1, stdout);
	}

	if(flush) {
		if(std::fflush(stdout) != 0) {
			out_buf_.clear();
			throw exception("Unable to flush log record to console");
		}
	}

	out_buf_.clear();

	if(out_buf_.capacity() > MAX_BUF_SIZE) [[unlikely]] {
		out_buf_.shrink_to_fit();
	}
}

void ConsoleSink::set_colour(Severity severity) {
	switch(severity) {
		case Severity::FATAL:
			[[fallthrough]];
		case Severity::ERROR_:
			[[fallthrough]];
		case Severity::WARN:
			util::set_output_colour(util::Colour::LIGHT_RED);
			break;
		case Severity::INFO:
			util::set_output_colour(util::Colour::WHITE);
			break;
		case Severity::DEBUG:
			util::set_output_colour(util::Colour::LIGHT_CYAN);
			break;
		case Severity::TRACE:
			util::set_output_colour(util::Colour::DARK_GREY);
			break;
		case Severity::DISABLED:
			// shutting the compiler up
			break;
	}
}

} // log, ember