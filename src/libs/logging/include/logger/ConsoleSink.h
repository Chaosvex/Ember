/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <logger/Sink.h>

namespace ember::log {

class ConsoleSink final : public Sink {
	static constexpr auto SV_RESERVE = 256u;

	bool colour_;

	void set_colour(Severity severity);
	void do_batch_write(const std::span<std::pair<RecordDetail, std::vector<char>>>& records);

public:
	ConsoleSink(Severity severity, Filter filter) : Sink(severity, filter), colour_(false) {}
	void write(Severity severity, Filter type, std::span<const char> record, bool flush) override;
	void batch_write(const std::span<std::pair<RecordDetail, std::vector<char>>>& records) override;
	void colourise(bool colourise) { colour_ = colourise; }
};

} // log, ember