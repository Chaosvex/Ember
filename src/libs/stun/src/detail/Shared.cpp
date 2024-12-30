/*
* Copyright (c) 2024 Ember
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <stun/detail/Shared.h>
#include <stun/Attributes.h>
#include <stun/Exception.h>
#include <spark/buffers/BufferAdaptor.h>
#include <spark/buffers/BinaryStream.h>
#include <shared/utility/FNVHash.h>
#include <boost/assert.hpp>
#include <boost/endian.hpp>
#include <botan/hash.h>
#include <botan/mac.h>
#include <botan/bigint.h>
#include <format>

namespace ember::stun::detail {

namespace be = boost::endian;

constexpr std::uint32_t make_uint32(std::uint8_t i0, std::uint8_t i1,
                                    std::uint8_t i2, std::uint8_t i3) {
	return ((static_cast<std::uint32_t>(i0) << 24) |
		(static_cast<std::uint32_t>(i1) << 16) |
		(static_cast<std::uint32_t>(i2) <<  8) |
		(static_cast<std::uint32_t>(i3)));
}

/* 
* If the FINGERPRINT attribute is present, we need to adjust the
* header length to exclude it (pretend it isn't there) and then
* hash everything upto the MESSAGE-INTEGRITY attribute
*/
void hmac_helper(std::span<const std::uint8_t> buffer,
                 Botan::MessageAuthenticationCode* hmac,
                 const std::size_t msgi_offset) {
	Header hdr = read_header(buffer);
	hdr.length -= FP_ATTR_LENGTH;
	hmac->update(buffer.data(), HEADER_LEN_OFFSET);
	hmac->update_be(hdr.length);
	hmac->update(buffer.data() + 4, msgi_offset - 4);
}

std::size_t attribute_offset(std::span<const std::uint8_t> buffer, Attributes attr) {
	spark::io::BufferAdaptor sba(buffer);
	spark::io::BinaryStream stream(sba);
	stream.skip(HEADER_LENGTH);

	const Header hdr = read_header(buffer);

	while((stream.total_read() - HEADER_LENGTH) < hdr.length) {
		const auto curr_offset = stream.total_read();
		Attributes curr_attr{};
		be::big_uint16_t length{};

		stream >> curr_attr;
		stream >> length;

		be::big_to_native_inplace(curr_attr);

		if(curr_attr == attr) {
			return curr_offset;
		}

		// attributes must be padded to four byte boundaries but
		// the padding bytes are not included in the len field
		if(auto mod = length % 4) {
			length += 4 - mod;
		}

		stream.skip(length);
	}

	return 0;
}

std::uint32_t fingerprint(std::span<const std::uint8_t> buffer, bool complete) {
	std::size_t offset = buffer.size();

	if(complete) {
		offset = attribute_offset(buffer, Attributes::FINGERPRINT);
	}

	std::array<std::uint8_t, 4> res;
	auto crc_func = Botan::HashFunction::create_or_throw("CRC32");
	BOOST_ASSERT_MSG(crc_func->output_length() == res.size(), "Bad checksum size");
	crc_func->update(buffer.data(), offset);
	crc_func->final(res.data());
	const auto crc32 = make_uint32(res[0], res[1], res[2], res[3]);
	return crc32 ^ 0x5354554e;
}

// not entirely compliant with the RFC because it's missing a salsprep impl
std::array<std::uint8_t, 20> msg_integrity(std::span<const std::uint8_t> buffer,
                                           std::span<const std::uint8_t> username,
                                           std::string_view realm,
                                           std::string_view password,
                                           bool complete) {
	auto msgi_offset = buffer.size_bytes();
	std::size_t fp_offset = 0;

	if(complete) {
		msgi_offset = attribute_offset(buffer, Attributes::MESSAGE_INTEGRITY);
		fp_offset = attribute_offset(buffer, Attributes::FINGERPRINT);

		if(!msgi_offset) {
			throw parse_error(Error::BUFFER_PARSE_ERROR,
				"MESSAGE-INTEGRITY not found, cannot calculate HMAC-SHA1");
		}
	}

	const std::string concat = std::format(":{}:{}", realm, password);
	std::array<std::uint8_t, 16> md5_res;
	auto hasher = Botan::HashFunction::create_or_throw("MD5");
	BOOST_ASSERT_MSG(hasher->output_length() == md5_res.size(), "Bad hash size");
	hasher->update(username.data(), username.size_bytes());
	hasher->update(reinterpret_cast<const std::uint8_t*>(concat.data()), concat.size());
	hasher->final(md5_res.data());

	std::array<std::uint8_t, 20> sha1_res;
	auto hmac = Botan::MessageAuthenticationCode::create_or_throw("HMAC(SHA-1)");
	BOOST_ASSERT_MSG(hmac->output_length() == sha1_res.size(), "Bad hash size");
	hmac->set_key(md5_res.data(), md5_res.size());

	if(fp_offset) {
		hmac_helper(buffer, hmac.get(), msgi_offset);
	} else {
		hmac->update(buffer.data(), msgi_offset);
	}

	hmac->final(sha1_res.data());
	return sha1_res;
}

// not entirely compliant with the RFC because it's missing a salsprep impl
std::array<std::uint8_t, 20> msg_integrity(std::span<const std::uint8_t> buffer,
                                           std::string_view password,
                                           bool complete) {
	auto msgi_offset = buffer.size_bytes();
	std::size_t fp_offset = 0;

	if(complete) {
		fp_offset = attribute_offset(buffer, Attributes::FINGERPRINT);
		msgi_offset = attribute_offset(buffer, Attributes::MESSAGE_INTEGRITY);

		if(!msgi_offset) {
			throw parse_error(Error::BUFFER_PARSE_ERROR,
				"MESSAGE-INTEGRITY not found, cannot calculate HMAC-SHA1");
		}
	}

	std::array<std::uint8_t, 20> res;
	auto hmac = Botan::MessageAuthenticationCode::create_or_throw("HMAC(SHA-1)");
	BOOST_ASSERT_MSG(hmac->output_length() == res.size(), "Bad hash size");
	hmac->set_key(reinterpret_cast<const std::uint8_t*>(password.data()), password.size());

	if(fp_offset) {
		hmac_helper(buffer, hmac.get(), msgi_offset);
	} else {
		hmac->update(buffer.data(), msgi_offset);
	}

	hmac->final(res.data());
	return res;
}

bool magic_cookie_present(std::span<const std::uint8_t> buffer) {
	spark::io::BufferAdaptor sba(buffer);
;	spark::io::BinaryStream stream(sba);

	Header header{};
	stream >> header.type;
	stream >> header.length;
	stream >> header.cookie;

	return header.cookie == MAGIC_COOKIE;
}

Header read_header(std::span<const std::uint8_t> buffer) try {
	spark::io::BufferAdaptor sba(buffer);
	spark::io::BinaryStream stream(sba);

	Header header{};
	stream >> header.type;
	stream >> header.length;

	if(magic_cookie_present(buffer)) {
		stream >> header.cookie;
		stream >> header.tx_id.id_5389;
	} else {
		stream >> header.tx_id.id_3489;
	}

	return header;
} catch(const spark::exception& e) {
	throw exception(Error::BUFFER_PARSE_ERROR, e.what());
}

std::size_t generate_key(const TxID& tx_id, RFCMode mode) {
	/*
	 * Hash the transaction ID to use as a key for future lookup.
	 * FNV is used because it's already in the project, not for any
	 * particular property. Odds of a collision are very low. 
	 */
	FNVHash fnv;

	if(mode == RFC3489) {
		fnv.update(tx_id.id_3489.begin(), tx_id.id_3489.end());
	} else {
		fnv.update(tx_id.id_5389.begin(), tx_id.id_5389.end());
	}

	return fnv.hash();
}

} // detail, stun, ember