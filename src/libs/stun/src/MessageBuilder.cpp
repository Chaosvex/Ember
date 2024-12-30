/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stun/MessageBuilder.h>
#include <stun/Exception.h>
#include <stun/detail/Shared.h>
#include <shared/utility/FNVHash.h>
#include <gsl/gsl>
#include <algorithm>
#include <random>

namespace ember::stun {

using namespace detail;

MessageBuilder::MessageBuilder(MessageType type, RFCMode mode)
	: vba_(buffer_), stream_(vba_) {
	Header header = build_header(type, mode);
	write_header(header);
	key_ = generate_key(header.tx_id, mode);
}

Header MessageBuilder::build_header(const MessageType type, const RFCMode mode) {
	Header header{};
	header.type = std::to_underlying(type);
	header.length = 0;

	if(mode != RFC3489) {
		header.cookie = MAGIC_COOKIE;
	}

	generate_tx_id(header);
	return header;
}

void MessageBuilder::write_padding(const std::size_t length) {
	// not efficient but it really doesn't matter
	for(std::size_t i = 0; i < length; ++i) {
		stream_ << std::uint8_t(0);
	}
}

be::big_uint16_t MessageBuilder::len_be(const std::size_t length) {
	return gsl::narrow<std::uint16_t>(length);
}

be::big_uint16_t MessageBuilder::attr_be(const Attributes attr) {
	return std::to_underlying(attr);
}

void MessageBuilder::generate_tx_id(Header& header) {
	std::random_device rd;
	std::mt19937 mt(rd());

	if(header.cookie == MAGIC_COOKIE) {
		std::ranges::generate(header.tx_id.id_5389, mt);
	} else {
		std::ranges::generate(header.tx_id.id_3489, mt);
	}
}

void MessageBuilder::write_header(Header& header) {
	stream_ << header.type;
	stream_ << header.length;

	if(header.cookie == MAGIC_COOKIE) {
		stream_ << header.cookie;
		stream_ << header.tx_id.id_5389;
	} else {
		stream_ << header.tx_id.id_3489;
	}
}

template<typename T>
void MessageBuilder::add_textual(Attributes attr, std::span<const T> value) {
	if(value.size_bytes() > 763) {
		throw exception(Error::BAD_ATTRIBUTE_DATA,
			"Data size exceeded maximum allowed for this attribute");
	}

	auto length = value.size_bytes();
	std::size_t pad_size = 0;

	if(auto mod = length % PADDING_ROUND) {
		pad_size = PADDING_ROUND - mod;
	}

	stream_ << attr_be(attr);
	stream_ << len_be(length);
	stream_.put(value.begin(), value.end());
	write_padding(pad_size);
}

void MessageBuilder::add_software(std::string_view value) {
	add_textual<const char>(Attributes::SOFTWARE, value);
}

void MessageBuilder::add_change_request(const bool ip, const bool port) {
	std::uint32_t value{};
	value |= ip? CHANGE_IP_MASK : 0;
	value |= port? CHANGE_PORT_MASK : 0;

	stream_ << attr_be(Attributes::CHANGE_REQUEST);
	stream_ << len_be(CHANGE_REQUEST_BODY_LEN);
	stream_ << be::big_uint32_t(value);
}

void MessageBuilder::add_response_port(std::uint16_t port) {
	stream_ << attr_be(Attributes::RESPONSE_PORT);
	stream_ << len_be(RESPONSE_PORT_BODY_LEN);
	stream_ << be::big_uint16_t(port);
	write_padding(2);
}

void MessageBuilder::set_header_length(const std::uint16_t length) {
	static_assert(stream_.can_write_seek());
	stream_.write_seek(spark::io::StreamSeek::SK_BUFFER_ABSOLUTE, HEADER_LEN_OFFSET);
	stream_ << len_be(length);
	stream_.write_seek(spark::io::StreamSeek::SK_BUFFER_ABSOLUTE, stream_.size());
}

void MessageBuilder::add_fingerprint() {
	set_header_length(read_header(buffer_).length + 8);
	const auto fp = fingerprint(buffer_, false);
	stream_ << attr_be(Attributes::FINGERPRINT);
	stream_ << len_be(FINGERPRINT_BODY_LEN);
	stream_ << be::big_uint32_t(fp);
}

void MessageBuilder::add_message_integrity(std::span<const std::uint8_t> hash) {
	stream_ << attr_be(Attributes::MESSAGE_INTEGRITY);
	stream_ << len_be(hash.size());
	stream_.put(hash.begin(), hash.end());
};

std::vector<std::uint8_t> MessageBuilder::final(bool fingerprint) {
	assert(!finalised_);
	finalised_ = true;

	set_header_length(buffer_.size() - HEADER_LENGTH);

	if(fingerprint) {
		add_fingerprint();
	}

	return std::move(buffer_);
}

std::vector<std::uint8_t> MessageBuilder::final(const char* const password,
                                                bool fingerprint) {
	return final(std::string_view(password), fingerprint);
}

std::vector<std::uint8_t> MessageBuilder::final(std::string_view password,
                                                bool fingerprint) {
	assert(!finalised_);
	finalised_ = true;

	set_header_length((buffer_.size() - HEADER_LENGTH) + 24);
	const auto hash = msg_integrity(buffer_, password, false);
	add_message_integrity(hash);

	if(fingerprint) {
		add_fingerprint();
	}

	set_header_length(buffer_.size() - HEADER_LENGTH);
	return std::move(buffer_);
}

std::vector<std::uint8_t> MessageBuilder::final(std::span<const std::uint8_t> username,
                                                std::string_view realm,
                                                std::string_view password,
                                                bool fingerprint) {
	assert(!finalised_);
	finalised_ = true;

	set_header_length((buffer_.size() - HEADER_LENGTH) + 24);
	const auto hash = msg_integrity(buffer_, username, realm, password, false);
	add_message_integrity(hash);

	if(fingerprint) {
		add_fingerprint();
	}

	set_header_length(buffer_.size() - HEADER_LENGTH);
	return std::move(buffer_);
}

std::size_t MessageBuilder::key() const {
	return key_;
}

} // stun, ember