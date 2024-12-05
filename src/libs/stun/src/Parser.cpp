/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stun/Parser.h>
#include <stun/detail/Shared.h>
#include <spark/buffers/BufferAdaptor.h>
#include <spark/buffers/BinaryStream.h>
#include <spark/buffers/pmr/BufferAdaptor.h>
#include <boost/assert.hpp>
#include <botan/hash.h>
#include <botan/mac.h>
#include <botan/bigint.h>
#include <span>
#include <stdexcept>

namespace ember::stun {

void Parser::set_logger(LogCB logger) {
	logger_ = logger;
}

Error Parser::validate_header(const Header& header) const {
	if(mode_ != RFCMode::RFC3489 && header.cookie != MAGIC_COOKIE) {
		return Error::RESP_COOKIE_MISSING;
	}

	if(header.length < ATTR_HEADER_LENGTH) {
		return Error::RESP_BAD_HEADER_LENGTH;
	}

	return Error::OK;
}

attributes::XorMappedAddress
Parser::xor_mapped_address(spark::io::pmr::BinaryStreamReader& stream, const TxID& id) {
	stream.skip(1); // skip reserved byte
	attributes::XorMappedAddress attr{};
	stream >> attr.family;

	// XOR port with the magic cookie
	stream >> attr.port;
	be::big_to_native_inplace(attr.port);
	attr.port ^= MAGIC_COOKIE >> 16;

	if(attr.family == AddressFamily::IPV4) {
		stream >> attr.ipv4;
		be::big_to_native_inplace(attr.ipv4);
		attr.ipv4 ^= MAGIC_COOKIE;
	} else if(attr.family == AddressFamily::IPV6 && mode_ != RFC3489) {
		stream.get(attr.ipv6.begin(), attr.ipv6.end());
		const std::uint32_t cookie[1]{ be::native_to_big(MAGIC_COOKIE) };
		const auto cookie_bytes = std::as_bytes(std::span(cookie));
		const auto tx_bytes = std::as_bytes(std::span(id.id_5389));
		const auto ipv6_bytes = std::as_writable_bytes(std::span(attr.ipv6));

		for(std::size_t i = 0; i < cookie_bytes.size(); ++i) {
			ipv6_bytes[i] ^= cookie_bytes[i];
		}

		for(std::size_t i = 0; i < tx_bytes.size(); ++i) {
			ipv6_bytes[i + sizeof(MAGIC_COOKIE)] ^= tx_bytes[i];
		}
	} else {
		throw parse_error(Error::RESP_ADDR_FAM_NOT_VALID,
			"Address family is not valid");
	}

	return attr;
}

attributes::Fingerprint Parser::fingerprint(spark::io::pmr::BinaryStreamReader& stream) {
	attributes::Fingerprint attr{};
	stream >> attr.crc32;
	be::big_to_native_inplace(attr.crc32);
	return attr;
}

Header Parser::read_header() try {
	spark::io::BufferAdaptor sba(buffer_);
	spark::io::BinaryStream stream(sba);

	Header header{};
	stream >> header.type;
	stream >> header.length;

	if(mode_ != RFCMode::RFC3489) {
		stream >> header.cookie;
		stream >> header.tx_id.id_5389;
	} else {
		stream >> header.tx_id.id_3489;
	}

	return header;
} catch(const spark::exception& e) {
	throw exception(Error::BUFFER_PARSE_ERROR, e.what());
}

attributes::MessageIntegrity
Parser::message_integrity(spark::io::pmr::BinaryStreamReader& stream) {
	attributes::MessageIntegrity attr{};
	stream.get(attr.hmac_sha1.begin(), attr.hmac_sha1.end());
	return attr;
}

attributes::MessageIntegrity256
Parser::message_integrity_sha256(spark::io::pmr::BinaryStreamReader& stream, const std::size_t length) {
	attributes::MessageIntegrity256 attr{};

	if (length < 16 || length > attr.hmac_sha256.size()) {
		throw parse_error(Error::RESP_BAD_HMAC_SHA_ATTR,
			"Invalid message integrity length");
	}

	stream.get(attr.hmac_sha256.begin(), attr.hmac_sha256.begin() + length);
	return attr;
}

attributes::Username
Parser::username(spark::io::pmr::BinaryStreamReader& stream, const std::size_t size) {
	attributes::Username attr{};
	attr.value.resize(size);
	stream.get(attr.value.begin(), attr.value.end());

	// must be padded to the nearest four bytes
	if(auto mod = size % 4) {
		stream.skip(4 - mod);
	}

	return attr;
}

attributes::ErrorCode
Parser::error_code(spark::io::pmr::BinaryStreamReader& stream, std::size_t length) {
	attributes::ErrorCode attr{};
	stream >> attr.code;

	be::big_to_native_inplace(attr.code);

	if(attr.code & 0xFFE00000) {
		logger_(Verbosity::STUN_LOG_DEBUG, Error::RESP_ERROR_CODE_OUT_OF_RANGE);
	}

	// (╯°□°）╯︵ ┻━┻
	const auto code = ((attr.code >> 8) & 0x07) * 100;
	const auto num = attr.code & 0xFF;

	if(code < 300 || code >= 700) {
		if(mode_ != RFCMode::RFC3489) {
			logger_(Verbosity::STUN_LOG_DEBUG, Error::RESP_ERROR_CODE_OUT_OF_RANGE);
		} else if(code < 100) { // original RFC has a wider range (1xx - 6xx)
			logger_(Verbosity::STUN_LOG_DEBUG, Error::RESP_ERROR_CODE_OUT_OF_RANGE);
		}
	}

	if(num >= 100) {
		logger_(Verbosity::STUN_LOG_DEBUG, Error::RESP_ERROR_CODE_OUT_OF_RANGE);
	}

	attr.code = code + num;

	attr.reason.resize_and_overwrite(length - sizeof(attributes::ErrorCode::code),
	                                 [&](char* strbuf, std::size_t size) {
		stream.get(strbuf, size);
		return size;
	});

	if(auto mod = attr.reason.size() % 4) {
		stream.skip(4 - mod);
	}

	return attr;
}

attributes::UnknownAttributes
Parser::unknown_attributes(spark::io::pmr::BinaryStreamReader& stream, std::size_t length) {
	if(length % 2) {
		throw parse_error(Error::RESP_UNK_ATTR_BAD_PAD,
			"Bad UNKNOWN-ATTRIBUTES length");
	}
	
	attributes::UnknownAttributes attr{};

	while(length) {
		Attributes attr_type;
		stream >> attr_type;
		be::big_to_native_inplace(attr_type);
		attr.attributes.emplace_back(attr_type);
		length -= sizeof(attr_type);
	}

	if(attr.attributes.size() % 2) {
		logger_(Verbosity::STUN_LOG_DEBUG, Error::RESP_UNK_ATTR_BAD_PAD);
	}

	return attr;
}

std::vector<attributes::Attribute> Parser::attributes() try {
	spark::io::pmr::BufferReadAdaptor sba(buffer_);
	spark::io::pmr::BinaryStreamReader stream(sba);
	stream.skip(HEADER_LENGTH);

	const Header hdr = read_header();
	const MessageType type { hdr.type.value() };

	std::vector<attributes::Attribute> attributes;
	bool has_msg_integrity = false;

	while((stream.total_read() - HEADER_LENGTH) < hdr.length) {
		auto attribute = extract_attribute(stream, hdr.tx_id, type);

		if(!attribute) {
			continue;
		}

		// if the MESSAGE-INTEGRITY attribute is present, we need to ignore
		// everything that follows except for the FINGERPRINT attribute
		if(has_msg_integrity && !std::get_if<attributes::Fingerprint>(&(*attribute))) {
			break;
		}

		if(!has_msg_integrity && std::get_if<attributes::MessageIntegrity>(&(*attribute))) {
			has_msg_integrity = true;
		}

		attributes.emplace_back(std::move(*attribute));
	}

	return attributes;
} catch(const spark::exception& e) {
	throw exception(Error::BUFFER_PARSE_ERROR, e.what());
}

std::optional<attributes::Attribute>
Parser::extract_attribute(spark::io::pmr::BinaryStreamReader& stream,
                          const TxID& id, const MessageType type) {
	Attributes attr_type;
	be::big_uint16_t length;
	stream >> attr_type;
	stream >> length;
	be::big_to_native_inplace(attr_type);

	const bool required = (std::to_underlying(attr_type) >> 15) ^ 1;
	const bool attr_valid = check_attr_validity(attr_type, type, required);

	if(!attr_valid && required) {
		throw parse_error(Error::RESP_UNEXPECTED_ATTR,
			"Encountered an unknown comprehension-required attribute");
	}

	switch(attr_type) {
		case Attributes::MAPPED_ADDRESS:
			return extract_ip_pair<attributes::MappedAddress>(stream);
		case Attributes::XOR_MAPPED_ADDR_OPT: // it's a faaaaake!
			[[fallthrough]];
		case Attributes::XOR_MAPPED_ADDRESS:
			return xor_mapped_address(stream, id);
		case Attributes::CHANGED_ADDRESS:
			return extract_ipv4_pair<attributes::ChangedAddress>(stream);
		case Attributes::SOURCE_ADDRESS:
			return extract_ipv4_pair<attributes::SourceAddress>(stream);
		case Attributes::OTHER_ADDRESS:
			return extract_ip_pair<attributes::OtherAddress>(stream);
		case Attributes::RESPONSE_ORIGIN:
			return extract_ip_pair<attributes::ResponseOrigin>(stream);
		case Attributes::REFLECTED_FROM:
			return extract_ipv4_pair<attributes::ReflectedFrom>(stream);
		case Attributes::RESPONSE_ADDRESS:
			return extract_ipv4_pair<attributes::ResponseAddress>(stream);
		case Attributes::MESSAGE_INTEGRITY:
			return message_integrity(stream);
		case Attributes::MESSAGE_INTEGRITY_SHA256:
			return message_integrity_sha256(stream, length);
		case Attributes::USERNAME:
			return username(stream, length);
		case Attributes::SOFTWARE:
			return extract_utf8_text<attributes::Software>(stream, length);
		case Attributes::ALTERNATE_SERVER:
			return extract_ip_pair<attributes::AlternateServer>(stream);
		case Attributes::FINGERPRINT:
			return fingerprint(stream);
		case Attributes::ERROR_CODE:
			return error_code(stream, length);
		case Attributes::UNKNOWN_ATTRIBUTES:
			return unknown_attributes(stream, length);
		case Attributes::REALM:
			return extract_utf8_text<attributes::Realm>(stream, length);
		case Attributes::NONCE:
			return extract_utf8_text<attributes::Nonce>(stream, length);
		case Attributes::PADDING:
			return extract_utf8_text<attributes::Padding>(stream, length);
		case Attributes::ICE_CONTROLLING:
			return ice_controlling(stream);
		case Attributes::ICE_CONTROLLED:
			return ice_controlled(stream);
		case Attributes::PRIORITY:
			return priority(stream);
		case Attributes::USE_CANDIDATE:
			return attributes::UseCandidate{};
	}

	stream.skip(length);
	return std::nullopt;
}

attributes::IceControlled Parser::ice_controlled(spark::io::pmr::BinaryStreamReader& stream) {
	attributes::IceControlled attr{};
	stream >> attr.value;
	be::big_to_native_inplace(attr.value);
	return attr;
}

attributes::IceControlling Parser::ice_controlling(spark::io::pmr::BinaryStreamReader& stream) {
	attributes::IceControlling attr{};
	stream >> attr.value;
	be::big_to_native_inplace(attr.value);
	return attr;
}

attributes::Priority Parser::priority(spark::io::pmr::BinaryStreamReader& stream) {
	attributes::Priority attr{};
	stream >> attr.value;
	be::big_to_native_inplace(attr.value);
	return attr;
}

bool Parser::check_attr_validity(const Attributes attr_type, const MessageType msg_type,
                                 const bool required) {
	/*
	 * If this attribute is marked as required, we'll look it up in the map
	 * to check whether we know what it is and more importantly whose fault
	 * it is if we can't finish parsing the message, given our current RFC mode
	 */
	if(required) {
		if(const auto rfc = attr_req_lut.find(attr_type); rfc != attr_req_lut.end()) {
			const auto res = std::ranges::find(rfc->second, mode_);

			if(res == rfc->second.end()) { // definitely not our fault... probably
				logger_(Verbosity::STUN_LOG_DEBUG, Error::RESP_BAD_REQ_ATTR_SERVER);
				return false;
			}
		} else {
			// might be our fault but probably not
			logger_(Verbosity::STUN_LOG_DEBUG, Error::RESP_UNKNOWN_REQ_ATTRIBUTE);
			return false;
		}
	}

	// if we're parsing a request, don't bother checking
	if(msg_type == MessageType::BINDING_REQUEST) {
		return true;
	}

	// Check whether this attribute is valid for the given response type
	if(const auto entry = attr_valid_lut.find(attr_type); entry != attr_valid_lut.end()) {
		if(entry->second != msg_type) { // not valid for this type
			logger_(Verbosity::STUN_LOG_DEBUG,
				required? Error::RESP_BAD_REQ_ATTR_SERVER : Error::RESP_UNKNOWN_OPT_ATTRIBUTE);
			return false;
		}
	} else { // not valid for *any* response type
		logger_(Verbosity::STUN_LOG_DEBUG,
			required? Error::RESP_BAD_REQ_ATTR_SERVER : Error::RESP_UNKNOWN_OPT_ATTRIBUTE);
		return false;
	}

	return true;
}

std::uint32_t Parser::fingerprint() const {
	return detail::fingerprint(buffer_, true);
}

std::array<std::uint8_t, 20> Parser::msg_integrity(std::span<const std::uint8_t> username,
                                                   std::string_view realm,
                                                   std::string_view password) const {
	return detail::msg_integrity(buffer_, username, realm, password, true);
}

std::array<std::uint8_t, 20> Parser::msg_integrity(std::string_view password) const {
	return detail::msg_integrity(buffer_, password, true);
}

} // stun, ember