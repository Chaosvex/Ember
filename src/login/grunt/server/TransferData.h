/*
 * Copyright (c) 2016 - 2020 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "../Opcodes.h"
#include "../Packet.h"
#include "../Exceptions.h"
#include <boost/assert.hpp>
#include <boost/endian/arithmetic.hpp>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace ember::grunt::server {

namespace be = boost::endian;

class TransferData final : public Packet {
	static const std::size_t WIRE_LENGTH = 1;
	State state_ = State::INITIAL;

public:
	static const std::uint16_t MAX_CHUNK_SIZE = 65535;

	TransferData() : Packet(Opcode::CMD_XFER_DATA) {}

	be::little_uint16_t size = 0;
	std::array<std::byte, MAX_CHUNK_SIZE> chunk;

	State read_from_stream(spark::BinaryStream& stream) override {
		BOOST_ASSERT_MSG(state_ != State::DONE, "Packet already complete - check your logic!");

		if(stream.size() < WIRE_LENGTH) {
			return State::CALL_AGAIN;
		}

		stream >> opcode;
		stream >> size;

		return (state_ = State::DONE);
	}

	void write_to_stream(spark::BinaryStream& stream) const override {
		stream << opcode;
		stream << size;
		stream.put(chunk.data(), size);
	}
};

} // client, grunt, ember