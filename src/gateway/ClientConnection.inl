/*
 * Copyright (c) 2018 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <spark/v2/buffers/BinaryStream.h>
#include <gsl/gsl_util>
#include <algorithm>

template<typename PacketType>
void ClientConnection::send(const PacketType& packet) {
	LOG_TRACE_FILTER(logger_, LF_NETWORK) << remote_address() << " <- "
		<< protocol::to_string(packet.opcode) << LOG_ASYNC;

	spark::v2::BinaryStream stream(*outbound_back_);
	stream << packet;

	const auto written = stream.total_write();
	auto size = gsl::narrow<typename PacketType::SizeType>(written - sizeof(typename PacketType::SizeType));
	auto opcode = packet.opcode;

	if(crypt_) [[likely]] {
		crypt_->encrypt(size);
		crypt_->encrypt(opcode);
	}

	stream.write_seek(spark::StreamSeek::SK_STREAM_ABSOLUTE, 0);
	stream << size << opcode;
	stream.write_seek(spark::StreamSeek::SK_FORWARD, written - PacketType::HEADER_WIRE_SIZE);

	if(!write_in_progress_) {
		write_in_progress_ = true;
		std::swap(outbound_front_, outbound_back_);
		write();
	}

	if(packet_logger_) [[unlikely]] {
		packet_logger_->log(packet, PacketDirection::OUTBOUND);
	}

	++stats_.messages_out;
}