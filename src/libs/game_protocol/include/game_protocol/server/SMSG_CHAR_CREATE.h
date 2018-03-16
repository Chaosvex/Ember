/*
 * Copyright (c) 2016 - 2018 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <game_protocol/Packet.h>
#include <game_protocol/ResultCodes.h>
#include <string>
#include <cstdint>
#include <cstddef>

namespace ember::protocol {

class SMSG_CHAR_CREATE final : public ServerPacket {
	State state_ = State::INITIAL;

public:
	SMSG_CHAR_CREATE() : ServerPacket(protocol::ServerOpcode::SMSG_CHAR_CREATE) { }

	Result result;
	
	State read_from_stream(spark::BinaryStream& stream) override try {
		BOOST_ASSERT_MSG(state_ != State::DONE, "Packet already complete - check your logic!");
		stream >> result;
		return (state_ = State::DONE);
	} catch(spark::exception&) {
		return State::ERRORED;
	}

	void write_to_stream(spark::BinaryStream& stream) const override {
		stream << result;
	}
};

} // protocol, ember
