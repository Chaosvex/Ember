/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <boost/endian/conversion.hpp>
#include <boost/uuid/uuid.hpp>
#include <stdexcept>
#include <cstdint>

namespace ember::spark {

namespace be = boost::endian;

class MessageHeader final {
	constexpr static auto MIN_HEADER_SIZE = 8u;

public:
	enum class State {
		INITIAL, ERRORED, OK
	} state = State::INITIAL;

	boost::uuids::uuid uuid = {};
	std::uint32_t size = 0;
	std::uint8_t channel = 0;
	std::uint8_t response = 0;
	std::uint8_t padding = 0;

private:
	std::uint8_t alignment_ = 0;

public:
	State read_from_stream(auto& stream) try {
		stream >> size;
		stream >> channel;
		stream >> response;

		std::uint8_t has_uuid = 0;
		stream >> has_uuid;

		if(has_uuid) {
			stream >> uuid;
		}

		stream >> padding;

		be::little_to_native_inplace(size);
		be::little_to_native_inplace(channel);
		be::little_to_native_inplace(padding);

		stream.skip(padding);
		return (state = State::OK);
	} catch(const std::exception&) {
		state = State::ERRORED;
		return state;
	}

	void write_to_stream(auto& stream) const {
		auto write_size = MIN_HEADER_SIZE;

		if(!uuid.is_nil()) {
			write_size += uuid.size();
		}

		auto pad = alignment_ - (write_size % alignment_);

		stream << be::native_to_little(size) + write_size + pad;
		stream << channel;
		stream << response;
		stream << static_cast<std::uint8_t>(!uuid.is_nil());
		
		if(!uuid.is_nil()) {
			stream << uuid;
		}

		stream << static_cast<std::uint8_t>(be::native_to_little(pad));

		// pad the header so the body starts at the correct alignment
		// the underlying buffer also needs to be properly aligned
		for(auto i = 0u; i < pad; ++i) {
			stream << std::uint8_t(0);
		}
	}

	void set_alignment(std::uint8_t alignment) {
		alignment_ = alignment;
	}
};

} // spark, ember