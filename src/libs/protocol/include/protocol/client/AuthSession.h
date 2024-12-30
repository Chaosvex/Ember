/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <spark/buffers/BinaryStream.h>
#include <spark/buffers/BufferAdaptor.h>
#include <protocol/Packet.h>
#include <logger/Logger.h>
#include <shared/utility/UTF8String.h>
#include <boost/assert.hpp>
#include <boost/endian/arithmetic.hpp>
#include <boost/container/small_vector.hpp>
#include <gsl/gsl_util>
#include <array>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <zlib.h>

namespace ember::protocol::client {

namespace be = boost::endian;

class AuthSession final {
	static const std::size_t DIGEST_LENGTH = 20;

	State state_ = State::INITIAL;

public:
	struct AddonData {
		std::string name;
		be::little_uint8_t key_version;
		be::little_uint32_t crc;
		be::little_uint32_t update_url_crc;
	};

	std::array<std::uint8_t, DIGEST_LENGTH> digest;
	be::little_uint32_t seed;
	be::little_uint32_t id;
	be::little_uint32_t security;
	be::little_uint32_t server_id;
	be::little_uint32_t build;
	be::little_uint8_t locale;
	utf8_string username;
	boost::container::small_vector<AddonData, 64> addons;

	State read_from_stream(auto& stream) try {
		BOOST_ASSERT_MSG(state_ != State::DONE, "Packet already complete - check your logic!");

		const auto initial_stream_size = stream.size();

		stream >> build;
		stream >> server_id;
		stream >> username;
		stream >> seed;
		stream.get(digest.data(), DIGEST_LENGTH);
		
		// handle compressed addon data
		be::little_uint32_t decompressed_size;
		stream >> decompressed_size;

		if(!stream.read_limit()) {
			LOG_ERROR_GLOB << "CMSG_AUTH_SESSION size not specified?" << LOG_ASYNC;
			return (state_ = State::ERRORED);
		}

		// calculate how many bytes are left in this message
		const auto remaining = stream.read_limit() - stream.total_read();
		const uLongf compressed_size = gsl::narrow<uLongf>(remaining);

		if(decompressed_size > 0xFFFFF) {
			LOG_DEBUG_GLOB << "Rejecting compressed addon data for being too large "  << LOG_ASYNC;
			return (state_ = State::ERRORED);
		}
		
		boost::container::small_vector<std::uint8_t, 512> source(
			remaining, boost::container::default_init
		);

		boost::container::small_vector<std::uint8_t, 4096> dest(
			decompressed_size, boost::container::default_init
		);

		uLongf dest_len = decompressed_size;
		stream.get(source.data(), remaining);
		
		auto ret = uncompress(dest.data(), &dest_len, source.data(), compressed_size);

		if(ret != Z_OK) {
			LOG_DEBUG_GLOB << "Decompression of addon data failed with code " << ret << LOG_ASYNC;
			return (state_ = State::ERRORED);
		}

		spark::io::BufferAdaptor buffer(dest);
		spark::io::BinaryStream addon_stream(buffer);

		while(!addon_stream.empty()) {
			AddonData data;
			addon_stream >> data.name;
			addon_stream >> data.key_version;
			addon_stream >> data.crc;
			addon_stream >> data.update_url_crc;
			addons.emplace_back(std::move(data));
		}

		return (state_ = State::DONE);
	} catch(const std::exception&) {
		return State::ERRORED;
	}

	void write_to_stream(auto& stream) const {
		stream << build;
		stream << server_id;
		stream << username;
		stream << seed;
		stream.put(digest.data(), digest.size());
	}
};

} // protocol, ember