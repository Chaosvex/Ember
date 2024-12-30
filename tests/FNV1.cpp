/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <shared/utility/FNVHash.h>
#include <boost/endian.hpp>
#include <gtest/gtest.h>
#include <array>
#include <string>
#include <string_view>
#include <cstdint>

using namespace ember;
namespace be = boost::endian;

TEST(FNV1, HelloWorld) {
	FNVHash fnv;
	fnv.update("Hello, world!");
	ASSERT_EQ(fnv.finalise(), 0xe84ead66);
	ASSERT_EQ(fnv.hash(), 0x811c9dc5);
}

TEST(FNV1, FinaliseReset) {
	FNVHash fnv;
	fnv.update("These two hashes should be equal");
	ASSERT_EQ(fnv.finalise(), 0x49ce320c);
	fnv.update("These two hashes should be equal");
	ASSERT_EQ(fnv.finalise(), 0x49ce320c);
}

TEST(FNV1, StringLiterals) {
	FNVHash fnv;
	fnv.update("The quick brown fox");
	ASSERT_EQ(fnv.hash(), 0xcb423604);
	fnv.update(" jumped over the lazy dog");
	ASSERT_EQ(fnv.finalise(), 0x007e0296);
}

TEST(FNV1, CStr) {
	const char* str = "The quick brown fox jumped over the lazy dog";
	FNVHash fnv;
	fnv.update(str);
	ASSERT_EQ(fnv.finalise(), 0x007e0296);
}

TEST(FNV1, STDString) {
	const std::string str("The quick brown fox jumped over the lazy dog");
	FNVHash fnv;
	fnv.update(str);
	ASSERT_EQ(fnv.finalise(), 0x007e0296);
}

TEST(FNV1, Stringview) {
	std::string_view str("The quick brown fox jumped over the lazy dog");
	FNVHash fnv;
	fnv.update(str.begin(), str.end());
	ASSERT_EQ(fnv.finalise(), 0x007e0296);
}

TEST(FNV1, ByteArray) {
	const std::array<std::uint8_t, 6> bytes { 0x01, 0xbe, 0xef, 0x13, 0x37, 0x9d };
	FNVHash fnv;
	fnv.update(bytes.begin(), bytes.end());
	ASSERT_EQ(fnv.finalise(), 0x70289e36);
}

TEST(FNV1, SingleBytes) {
	const std::array<std::uint8_t, 6> bytes { 0x01, 0xbe, 0xef, 0x13, 0x37, 0x9d };
	FNVHash fnv;
	fnv.update(bytes[0]);
	fnv.update(bytes[1]);
	fnv.update(bytes[2]);
	fnv.update(bytes[3]);
	fnv.update(bytes[4]);
	fnv.update(bytes[5]);
	ASSERT_EQ(fnv.finalise(), 0x70289e36);
}

TEST(FNV1, Integers) {
	FNVHash fnv;
	fnv.update_be(std::uint8_t(0xff));
	ASSERT_EQ(fnv.hash(), 0x050c5de0);
	fnv.update_be(std::uint16_t(0xbeef));
	ASSERT_EQ(fnv.hash(), 0x708e74d5);
	fnv.update_be(std::uint32_t(0x1badb002));
	ASSERT_EQ(fnv.hash(), 0xb627678b);
	fnv.update_be(std::uint32_t(0x8badf00d));
	ASSERT_EQ(fnv.hash(), 0xf61748e6);
	fnv.update_be(std::uint64_t(0xfeedfacecafebeef));
	ASSERT_EQ(fnv.hash(), 0x715b9f10);
}

TEST(FNV1, EndianEquality) {
	FNVHash fnv;
	const auto le_int { be::native_to_little(0xf0cacc1a) };
	const auto be_int { be::native_to_big(0xf0cacc1a) };
	fnv.update_be(le_int);
	ASSERT_EQ(fnv.finalise(), 0x48d36a41);
	fnv.update_le(be_int);
	ASSERT_EQ(fnv.finalise(), 0x48d36a41);
}

TEST(FNV1, MixedTypes) {
	FNVHash fnv;
	const char* str = "String";
	const std::array<std::uint8_t, 2> bytes {0xff, 0xaa};
	const std::uint8_t byte = 0xaa;
	fnv.update(str);
	fnv.update(bytes.begin(), bytes.end());
	fnv.update(byte);
	ASSERT_EQ(fnv.finalise(), 0x445bce95);
}