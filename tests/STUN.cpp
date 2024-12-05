/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "STUNVectors.h"
#include <spark/buffers/pmr/BinaryStream.h>
#include <stun/Client.h>
#include <stun/Parser.h>
#include <stun/Utility.h>
#include <stun/MessageBuilder.h>
#include <gtest/gtest.h>
#include <array>
#include <variant>
#include <cstddef>

using namespace ember;

TEST(STUNVectors, RFC5769_IPv4Response) {
	std::span span(respv4);
	stun::Parser parser(span, stun::RFC5780);
	const auto header = parser.read_header();

	ASSERT_EQ(header.length, 60);
	ASSERT_EQ(static_cast<std::uint16_t>(header.type),
		std::to_underlying(stun::MessageType::BINDING_RESPONSE));
	ASSERT_EQ(header.cookie, stun::MAGIC_COOKIE);

	std::array<uint8_t, 12> parse_tx_id{}, trans_id{
		0xb7, 0xe7, 0xa7, 0x01, 0xbc, 0x34,
		0xd6, 0x86, 0xfa, 0x87, 0xdf, 0xae
	};

	memcpy(parse_tx_id.data(), header.tx_id.id_5389.data(), 12);
	ASSERT_EQ(trans_id, parse_tx_id);
	
	const auto attrs = parser.attributes();

	// SOFTWARE
	const auto software = stun::retrieve_attribute<stun::attributes::Software>(attrs);
	ASSERT_TRUE(software);
	ASSERT_EQ(software->value, "test vector");

	// XOR-MAPPED-ADDRESS
	const auto xma = stun::retrieve_attribute<stun::attributes::XorMappedAddress>(attrs);
	ASSERT_TRUE(xma);
	ASSERT_EQ(xma->family, stun::AddressFamily::IPV4);
	ASSERT_EQ(xma->port, 32853);
	ASSERT_EQ(xma->ipv4, 0x0c0000201);
	ASSERT_EQ(stun::extract_ip_to_string(*xma), "192.0.2.1");

	// FINGERPRINT
	const auto fp = stun::retrieve_attribute<stun::attributes::Fingerprint>(attrs);
	ASSERT_TRUE(fp);
	ASSERT_EQ(fp->crc32, 0xc07d4c96);
	ASSERT_EQ(fp->crc32, parser.fingerprint());

	// MESSAGE-INTEGRITY
	const auto msgi = stun::retrieve_attribute<stun::attributes::MessageIntegrity>(attrs);
	ASSERT_TRUE(msgi);

	const std::array<std::uint8_t, 20> hmac_sha1 {
		0x2b, 0x91, 0xf5, 0x99, 0xfd, 0x9e, 0x90, 0xc3, 0x8c, 0x74,
		0x89, 0xf9, 0x2a, 0xf9, 0xba, 0x53, 0xf0, 0x6b, 0xe7, 0xd7
	};

	ASSERT_EQ(msgi->hmac_sha1, hmac_sha1);
	const auto res = parser.msg_integrity("VOkJxbRl1RmTxUk/WvJxBt");
	ASSERT_TRUE(std::ranges::equal(msgi->hmac_sha1, res));
}

TEST(STUNVectors, RFC5769_IPv6Response) {
	std::span span(respv6);
	stun::Parser parser(span, stun::RFC5780);
	const auto header = parser.read_header();

	ASSERT_EQ(header.length, 72);
	ASSERT_EQ(static_cast<std::uint16_t>(header.type),
		std::to_underlying(stun::MessageType::BINDING_RESPONSE));
	ASSERT_EQ(header.cookie, stun::MAGIC_COOKIE);

	std::array<uint8_t, 12> parse_tx_id{}, trans_id {
		0xb7, 0xe7, 0xa7, 0x01, 0xbc, 0x34,
		0xd6, 0x86, 0xfa, 0x87, 0xdf, 0xae 
	};

	memcpy(parse_tx_id.data(), header.tx_id.id_5389.data(), sizeof(trans_id));
	ASSERT_EQ(trans_id, parse_tx_id);
	
	const auto attrs = parser.attributes();

	// SOFTWARE
	const auto software = stun::retrieve_attribute<stun::attributes::Software>(attrs);
	ASSERT_TRUE(software);
	ASSERT_EQ(software->value, "test vector");

	// XOR-MAPPED-ADDRESS
	const auto xma = stun::retrieve_attribute<stun::attributes::XorMappedAddress>(attrs);
	ASSERT_TRUE(xma);
	ASSERT_EQ(xma->family, stun::AddressFamily::IPV6);
	ASSERT_EQ(xma->port, 32853);
	ASSERT_EQ(stun::extract_ip_to_string(*xma), "2001:db8:1234:5678:11:2233:4455:6677");

	// FINGERPRINT
	const auto fp = stun::retrieve_attribute<stun::attributes::Fingerprint>(attrs);
	ASSERT_TRUE(fp);
	ASSERT_EQ(fp->crc32, 0xc8fb0b4c);
	ASSERT_EQ(fp->crc32, parser.fingerprint());

	// MESSAGE-INTEGRITY
	const auto msgi = stun::retrieve_attribute<stun::attributes::MessageIntegrity>(attrs);
	ASSERT_TRUE(msgi);

	const std::array<std::uint8_t, 20> hmac_sha1 {
		0xa3, 0x82, 0x95, 0x4e, 0x4b, 0xe6, 0x7b, 0xf1, 0x17, 0x84,
		0xc9, 0x7c, 0x82, 0x92, 0xc2, 0x75, 0xbf, 0xe3, 0xed, 0x41
	};

	ASSERT_EQ(msgi->hmac_sha1, hmac_sha1);
	const auto res = parser.msg_integrity("VOkJxbRl1RmTxUk/WvJxBt");
	ASSERT_TRUE(std::ranges::equal(msgi->hmac_sha1, res));
}

TEST(STUNVectors, RFC5769_LTARequest) {
	std::span span(reqltc);
	stun::Parser parser(span, stun::RFC5780);
	const auto header = parser.read_header();

	ASSERT_EQ(header.length, 96);
	ASSERT_EQ(static_cast<std::uint16_t>(header.type),
		std::to_underlying(stun::MessageType::BINDING_REQUEST));
	ASSERT_EQ(header.cookie, stun::MAGIC_COOKIE);

	std::array<uint8_t, 12> parse_tx_id{}, trans_id{
		0x78, 0xad, 0x34, 0x33, 0xc6, 0xad,
		0x72, 0xc0, 0x29, 0xda, 0x41, 0x2e
	};

	memcpy(parse_tx_id.data(), header.tx_id.id_5389.data(), 12);
	ASSERT_EQ(trans_id, parse_tx_id);
	
	const auto attrs = parser.attributes();

	// USERNAME
	const auto username = stun::retrieve_attribute<stun::attributes::Username>(attrs);
	ASSERT_TRUE(username);

	// NONCE
	const auto nonce = stun::retrieve_attribute<stun::attributes::Nonce>(attrs);
	ASSERT_TRUE(nonce);
	ASSERT_EQ(nonce->value, "f//499k954d6OL34oL9FSTvy64sA");

	// REALM
	const auto realm = stun::retrieve_attribute<stun::attributes::Realm>(attrs);
	ASSERT_TRUE(realm);
	ASSERT_EQ(realm->value, "example.org");

	// MESSAGE-INTEGRITY
	const auto msgi = stun::retrieve_attribute<stun::attributes::MessageIntegrity>(attrs);
	ASSERT_TRUE(msgi);
	
	const std::array<std::uint8_t, 20> hmac_sha1 {
		0xf6, 0x70, 0x24, 0x65, 0x6d, 0xd6, 0x4a, 0x3e, 0x02, 0xb8,
		0xe0, 0x71, 0x2e, 0x85, 0xc9, 0xa2, 0x8c, 0xa8, 0x96, 0x66
	};

	ASSERT_EQ(msgi->hmac_sha1, hmac_sha1);
	const auto res = parser.msg_integrity(username->value, realm->value, "TheMatrIX");
	ASSERT_TRUE(std::ranges::equal(msgi->hmac_sha1, res));
}

TEST(STUNVectors, RFC5769_Request) {
	std::span span(req);
	stun::Parser parser(span, stun::RFC8445);
	const auto header = parser.read_header();

	ASSERT_EQ(header.length, 88);
	ASSERT_EQ(static_cast<std::uint16_t>(header.type),
			  std::to_underlying(stun::MessageType::BINDING_REQUEST));
	ASSERT_EQ(header.cookie, stun::MAGIC_COOKIE);

	std::array<uint8_t, 12> parse_tx_id{}, trans_id{
		0xb7, 0xe7, 0xa7, 0x01, 0xbc, 0x34,
		0xd6, 0x86, 0xfa, 0x87, 0xdf, 0xae
	};

	memcpy(parse_tx_id.data(), header.tx_id.id_5389.data(), 12);
	ASSERT_EQ(trans_id, parse_tx_id);

	const auto attrs = parser.attributes();

	// USERNAME
	const auto username = stun::retrieve_attribute<stun::attributes::Username>(attrs);
	ASSERT_TRUE(username);
	const std::string expected_username = "evtj:h6vY";
	ASSERT_TRUE(std::ranges::equal(expected_username, username->value));

	// SOFTWARE
	const auto software = stun::retrieve_attribute<stun::attributes::Software>(attrs);
	ASSERT_TRUE(software);
	ASSERT_EQ(software->value, "STUN test client");

	// PRORITY
	const auto priority = stun::retrieve_attribute<stun::attributes::Priority>(attrs);
	ASSERT_TRUE(priority);
	ASSERT_EQ(priority->value, 0x6e0001ff);

	// ICE-CONTROLLED
	const auto controlled = stun::retrieve_attribute<stun::attributes::IceControlled>(attrs);
	ASSERT_TRUE(controlled);
	ASSERT_EQ(controlled->value, 0x932ff9b151263B36);

	// MESSAGE-INTEGRITY
	const auto msgi = stun::retrieve_attribute<stun::attributes::MessageIntegrity>(attrs);
	ASSERT_TRUE(msgi);

	const std::array<std::uint8_t, 20> hmac_sha1 {
		0x9a, 0xea, 0xa7, 0x0c, 0xbf, 0xd8, 0xcb, 0x56, 0x78, 0x1e,
		0xf2, 0xb5, 0xb2, 0xd3, 0xf2, 0x49, 0xc1, 0xb5, 0x71, 0xa2
	};

	ASSERT_EQ(msgi->hmac_sha1, hmac_sha1);

	const auto res = parser.msg_integrity("VOkJxbRl1RmTxUk/WvJxBt");
	ASSERT_TRUE(std::ranges::equal(msgi->hmac_sha1, res));

	// FINGERPRINT
	const auto fp = stun::retrieve_attribute<stun::attributes::Fingerprint>(attrs);
	ASSERT_TRUE(fp);
	ASSERT_EQ(fp->crc32, 0xe57a3bcf);
	ASSERT_EQ(fp->crc32, parser.fingerprint());
}

TEST(STUNVectors, Builder_CRC32) {
	stun::MessageBuilder builder(stun::MessageType::BINDING_REQUEST, stun::RFC5780);
	const auto buffer = builder.final(true);

	stun::Parser parser(buffer, stun::RFC5780);
	const auto attributes = parser.attributes();

	const auto fp = stun::retrieve_attribute<stun::attributes::Fingerprint>(attributes);
	ASSERT_TRUE(fp);
	ASSERT_EQ(fp->crc32, parser.fingerprint());
}

TEST(STUNVectors, Builder_MessageIntegrity) {
	stun::MessageBuilder builder(stun::MessageType::BINDING_REQUEST, stun::RFC5780);
	const auto buffer = builder.final("Banana");

	stun::Parser parser(buffer, stun::RFC5780);
	const auto attributes = parser.attributes();

	const auto attr = stun::retrieve_attribute<stun::attributes::MessageIntegrity>(attributes);
	ASSERT_TRUE(attr);
	const auto calc = parser.msg_integrity("Banana");
	ASSERT_TRUE(std::ranges::equal(attr->hmac_sha1, calc));
}

TEST(STUNVectors, Builder_MessageIntegrityFull) {
	stun::MessageBuilder builder(stun::MessageType::BINDING_REQUEST, stun::RFC5780);
	std::array<std::uint8_t, 8> username { 0x63, 0x68, 0x61, 0x6F, 0x73, 0x76, 0x65, 0x78 };
	const auto buffer = builder.final(username, "lightshope.org", "bAhzJk!/kM");

	stun::Parser parser(buffer, stun::RFC5780);
	const auto attributes = parser.attributes();

	const auto attr = stun::retrieve_attribute<stun::attributes::MessageIntegrity>(attributes);
	ASSERT_TRUE(attr);
	const auto calc = parser.msg_integrity(username, "lightshope.org", "bAhzJk!/kM");
	ASSERT_TRUE(std::ranges::equal(attr->hmac_sha1, calc));
}

TEST(STUNVectors, Builder_MessageIntegrityCRC32) {
	stun::MessageBuilder builder(stun::MessageType::BINDING_REQUEST, stun::RFC5780);
	const auto buffer = builder.final("Banana", true);

	stun::Parser parser(buffer, stun::RFC5780);
	const auto attributes = parser.attributes();

	// FINGERPRINT
	const auto fp = stun::retrieve_attribute<stun::attributes::Fingerprint>(attributes);
	ASSERT_TRUE(fp);
	ASSERT_EQ(fp->crc32, parser.fingerprint());

	// MESSAGE-INTEGRITY
	const auto attr = stun::retrieve_attribute<stun::attributes::MessageIntegrity>(attributes);
	ASSERT_TRUE(attr);
	const auto calc = parser.msg_integrity("Banana");
	ASSERT_TRUE(std::ranges::equal(attr->hmac_sha1, calc));
}

TEST(STUNVectors, Builder_Software) {
	stun::MessageBuilder builder(stun::MessageType::BINDING_REQUEST, stun::RFC5780);
	builder.add_software("Ember!");
	const auto buffer = builder.final(true);
	stun::Parser parser(buffer, stun::RFC5780);
	const auto attributes = parser.attributes();

	// SOFTWARE
	const auto attr = stun::retrieve_attribute<stun::attributes::Software>(attributes);
	ASSERT_TRUE(attr);
	ASSERT_EQ(attr->value, "Ember!");

	// FINGERPRINT
	const auto fp = stun::retrieve_attribute<stun::attributes::Fingerprint>(attributes);
	ASSERT_TRUE(fp);
	ASSERT_EQ(fp->crc32, parser.fingerprint());
}