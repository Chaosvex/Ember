/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "../Opcodes.h"
#include "../Packet.h"
#include "../Magic.h"
#include "../Exceptions.h"
#include "../ResultCodes.h"
#include "../../GameVersion.h"
#include <shared/utility/UTF8String.h>
#include <boost/assert.hpp>
#include <boost/endian/arithmetic.hpp>
#include <boost/endian/conversion.hpp>
#include <gsl/gsl_util>
#include <string>
#include <cstdint>
#include <cstddef>

namespace ember::grunt::client {

namespace be = boost::endian;

class LoginChallenge final : public Packet {
	static const std::size_t MAX_USERNAME_LEN = 16;
	static const std::size_t HEADER_LENGTH = 4;

	State state_ = State::INITIAL;
	std::uint8_t username_len_ = 0;

	void read_header(spark::io::pmr::BinaryStream& stream) {
		if(stream.size() < HEADER_LENGTH) {
			return;
		}

		stream >> opcode;
		stream >> protocol_ver;
		stream >> body_size;
		be::little_to_native_inplace(body_size);
	}

	void read_body(spark::io::pmr::BinaryStream& stream) {
		if(stream.size() < body_size) {
			state_ = State::CALL_AGAIN;
			return;
		}

		stream >> game;
		stream >> version.major;
		stream >> version.minor;
		stream >> version.patch;
		stream >> version.build;
		stream >> platform;
		stream >> os;
		stream >> locale;
		stream >> timezone_bias;
		stream >> ip;
		stream >> username_len_;

		if(username_len_ > MAX_USERNAME_LEN) {
			throw bad_packet("Username length was too long!");
		}

		if(stream.size() >= username_len_) {
			username.resize_and_overwrite(username_len_, [&](char* strlen, std::size_t size) {
				stream.get(strlen, size);
				return size;
			});
		} else {
			throw bad_packet("Invalid username length supplied!");
		}

		// handle endianness
		be::little_to_native_inplace(game);
		be::little_to_native_inplace(version.build);
		be::little_to_native_inplace(platform);
		be::little_to_native_inplace(os);
		be::little_to_native_inplace(locale);
		state_ = State::DONE;
	}

public:
	LoginChallenge() : Packet(Opcode::CMD_AUTH_LOGON_CHALLENGE) {}

	const static int CHALLENGE_VER = 3;
	const static int RECONNECT_CHALLENGE_VER = 2;

	std::uint8_t protocol_ver = 0;
	std::uint16_t body_size = 0;
	Game game;
	GameVersion version = {};
	Platform platform;
	System os;
	Locale locale;
	be::little_int32_t timezone_bias = 0;
	be::big_uint32_t ip = 0; // todo - apparently flipped with Mac builds (PPC only?)
	utf8_string username;

	State read_from_stream(spark::io::pmr::BinaryStream& stream) override {
		BOOST_ASSERT_MSG(state_ != State::DONE, "Packet already complete - check your logic!");

		switch(state_) {
			case State::INITIAL:
				read_header(stream);
				[[fallthrough]];
			case State::CALL_AGAIN:
				read_body(stream);
				break;
			default:
				BOOST_ASSERT_MSG(false, "Unreachable condition hit");
		}

		return state_;
	}

	void write_to_stream(spark::io::pmr::BinaryStream& stream) const override {
		if(username.length() > MAX_USERNAME_LEN) {
			throw bad_packet("Provided username was too long!");
		}

		const auto start_pos = stream.total_write();
		stream << opcode;
		stream << protocol_ver;
		const auto size_pos = stream.total_write();
		stream << std::uint16_t(0); // size placeholder
		stream << be::native_to_little(game);
		stream << version.major;
		stream << version.minor;
		stream << version.patch;
		stream << be::native_to_little(version.build);
		stream << be::native_to_little(platform);
		stream << be::native_to_little(os);
		stream << be::native_to_little(locale);
		stream << timezone_bias;
		stream << ip;
		stream << gsl::narrow<std::uint8_t>(username.length());
		stream.put(username.data(), username.length());
		const auto end_pos = stream.total_write();
		const auto size = (end_pos - start_pos) - HEADER_LENGTH;

		stream.write_seek(spark::io::StreamSeek::SK_STREAM_ABSOLUTE, size_pos);
		stream << be::native_to_little(gsl::narrow<std::uint16_t>(size));
		stream.write_seek(spark::io::StreamSeek::SK_FORWARD, size);
	}

};

using ReconnectChallenge = LoginChallenge;

} // client, grunt, ember