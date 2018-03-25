/*
 * Copyright (c) 2018 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "FBSink.h"
#include <PacketLog_generated.h>
#include <logger/Utility.h>
#include <boost/endian/conversion.hpp>
#include <memory>
#include <cstddef>
#include <ctime>

namespace be = boost::endian;

namespace ember {

FBSink::FBSink(const std::string& filename, const std::string& host,
               const std::string& remote_host) {
	start_log(filename, host, remote_host);
}

void FBSink::start_log(const std::string& filename, const std::string& host,
                       const std::string& remote_host) {
	file_ = std::ofstream(filename, std::fstream::binary | std::fstream::app | std::fstream::out);

	if(!file_.is_open()) {
		throw std::runtime_error("Unable to open file for packet logging");
	}

	flatbuffers::FlatBufferBuilder fbb;

	auto fb_host = fbb.CreateString(host);
	auto fb_remote = fbb.CreateString(remote_host);
	auto fb_time_fmt = fbb.CreateString(time_fmt_);
	auto fb_host_desc = fbb.CreateString("unused");

	fblog::HeaderBuilder hb(fbb);
	hb.add_version(VERSION);
	hb.add_host(fb_host);
	hb.add_host_desc(fb_host_desc);
	hb.add_remote_host(fb_remote);
	hb.add_time_format(fb_time_fmt);
	auto header = hb.Finish();
	fbb.Finish(header);

	const auto size = fbb.GetSize();
	const auto size_le = be::native_to_little(size);
	const auto type_le = be::native_to_little(static_cast<std::uint32_t>(fblog::Type::HEADER));

	file_.write(reinterpret_cast<const char*>(&size_le), sizeof(size_le));
	file_.write(reinterpret_cast<const char*>(&type_le), sizeof(type_le));
	file_.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), size);
}

void FBSink::log(const spark::Buffer& buffer, std::size_t length, const std::time_t& time, 
                 PacketDirection dir) {
	std::tm utc_time;

#if _MSC_VER && !__INTEL_COMPILER
	gmtime_s(&utc_time, &time);
#else
	gmtime_r(&time, &utc_time);
#endif

	auto t_buff = std::make_unique<std::uint8_t[]>(length);
	buffer.copy(t_buff.get(), length);

	flatbuffers::FlatBufferBuilder fbb;
	const auto payload = fbb.CreateVector(t_buff.get(), length);
	const auto time_str = log::detail::put_time(utc_time, time_fmt_);
	const auto fbtime = fbb.CreateString(time_str);
	const auto fbdir = dir == PacketDirection::INBOUND?
		fblog::Direction::INBOUND : fblog::Direction::OUTBOUND;

	fblog::MessageBuilder mb(fbb);
	mb.add_time(fbtime);
	mb.add_direction(fbdir);
	mb.add_payload(payload);
	auto message = mb.Finish();
	fbb.Finish(message);

	const auto size = fbb.GetSize();
	const auto size_le = be::native_to_little(size);
	const auto type_le = be::native_to_little(static_cast<std::uint32_t>(fblog::Type::MESSAGE));

	file_.write(reinterpret_cast<const char*>(&size_le), sizeof(size_le));
	file_.write(reinterpret_cast<const char*>(&type_le), sizeof(type_le));
	file_.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), size);
	file_.flush();
}

} // ember