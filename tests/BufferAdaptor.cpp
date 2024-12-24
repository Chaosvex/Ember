/*
* Copyright (c) 2024 Ember
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <spark/buffers/BufferAdaptor.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <span>
#include <string_view>
#include <vector>
#include <cstdint>

using namespace ember;
using namespace std::literals;

TEST(BufferAdaptor, SizeEmptyInitial) {
	std::array<std::uint8_t, 0> buffer;
	spark::io::BufferAdaptor adaptor(buffer);
	ASSERT_EQ(adaptor.size(), 0);
}

TEST(BufferAdaptor, Empty) {
	std::vector<std::uint8_t> buffer;
	spark::io::BufferAdaptor adaptor(buffer);
	ASSERT_TRUE(adaptor.empty());
	buffer.emplace_back(1);
	adaptor.advance_write(1);
	ASSERT_FALSE(adaptor.empty());
}

TEST(BufferAdaptor, SizePopulatedInitial) {
	std::array<std::uint8_t, 1> buffer { 1 };
	spark::io::BufferAdaptor adaptor(buffer);
	ASSERT_EQ(adaptor.size(), buffer.size());
}

TEST(BufferAdaptor, ResizeMatch) {
	std::vector<std::uint8_t> buffer { 1, 2, 3, 4, 5 };
	spark::io::BufferAdaptor adaptor(buffer);
	ASSERT_EQ(adaptor.size(), buffer.size());
	buffer.emplace_back(6);
	ASSERT_EQ(adaptor.size(), buffer.size());
}

TEST(BufferAdaptor, ReadOne) {
	std::array<std::uint8_t, 3> buffer { 1, 2, 3 };
	spark::io::BufferAdaptor adaptor(buffer);
	std::uint8_t value = 0;
	adaptor.read(&value, 1);
	ASSERT_EQ(adaptor.size(), buffer.size() - 1);
	ASSERT_EQ(value, 1);
}

TEST(BufferAdaptor, ReadAll) {
	std::array<std::uint8_t, 3> buffer { 1, 2, 3 };
	spark::io::BufferAdaptor adaptor(buffer);
	std::array<std::uint8_t, 3> values{};
	adaptor.read(&values, values.size());
	ASSERT_TRUE(std::ranges::equal(buffer, values));
}

TEST(BufferAdaptor, SingleSkipRead) {
	std::array<std::uint8_t, 3> buffer { 1, 2, 3 };
	spark::io::BufferAdaptor adaptor(buffer);
	std::uint8_t value = 0;
	adaptor.skip(1);
	adaptor.read(&value, 1);
	ASSERT_EQ(adaptor.size(), 1);
	ASSERT_EQ(value, buffer[1]);
}

TEST(BufferAdaptor, MultiskipRead) {
	std::array<std::uint8_t, 6> buffer { 1, 2, 3, 4, 5, 6 };
	spark::io::BufferAdaptor adaptor(buffer);
	std::uint8_t value = 0;
	adaptor.skip(5);
	adaptor.read(&value, 1);
	ASSERT_TRUE(adaptor.empty());
	ASSERT_EQ(value, buffer[5]);
}

TEST(BufferAdaptor, Write) {
	std::vector<std::uint8_t> buffer;
	spark::io::BufferAdaptor adaptor(buffer);
	std::array<std::uint8_t, 6> values { 1, 2, 3, 4, 5, 6 };
	adaptor.write(values.data(), values.size());
	ASSERT_EQ(adaptor.size(), values.size());
	ASSERT_EQ(buffer.size(), values.size());
	ASSERT_TRUE(std::ranges::equal(values, buffer));
}

TEST(BufferAdaptor, WriteAppend) {
	std::vector<std::uint8_t> buffer { 1, 2, 3 };
	spark::io::BufferAdaptor adaptor(buffer);
	std::array<std::uint8_t, 3> values { 4, 5, 6 };
	adaptor.write(values.data(), values.size());
	ASSERT_EQ(buffer.size(), 6);
	ASSERT_EQ(adaptor.size(), buffer.size());
	std::array<std::uint8_t, 6> expected { 1, 2, 3, 4, 5, 6 };
	ASSERT_TRUE(std::ranges::equal(expected, buffer));
}

TEST(BufferAdaptor, CanWriteSeek) {
	std::vector<std::uint8_t> buffer;
	spark::io::BufferAdaptor adaptor(buffer);
	ASSERT_TRUE(adaptor.can_write_seek());
}

TEST(BufferAdaptor, WriteSeekBack) {
	std::vector<std::uint8_t> buffer { 1, 2, 3 };
	spark::io::BufferAdaptor adaptor(buffer);
	std::array<std::uint8_t, 3> values { 4, 5, 6 };
	adaptor.write_seek(spark::io::BufferSeek::SK_BACKWARD, 2);
	adaptor.write(values.data(), values.size());
	ASSERT_EQ(buffer.size(), 4);
	ASSERT_EQ(adaptor.size(), buffer.size());
	std::array<std::uint8_t, 4> expected { 1, 4, 5, 6 };
	ASSERT_TRUE(std::ranges::equal(expected, buffer));
}

TEST(BufferAdaptor, WriteSeekStart) {
	std::vector<std::uint8_t> buffer { 1, 2, 3 };
	spark::io::BufferAdaptor adaptor(buffer);
	std::array<std::uint8_t, 3> values { 4, 5, 6 };
	adaptor.write_seek(spark::io::BufferSeek::SK_ABSOLUTE, 0);
	adaptor.write(values.data(), values.size());
	ASSERT_EQ(buffer.size(), values.size());
	ASSERT_EQ(adaptor.size(), buffer.size());
	ASSERT_TRUE(std::ranges::equal(buffer, values));
}

TEST(BufferAdaptor, ReadPtr) {
	std::array<std::uint8_t, 3> buffer { 1, 2, 3 };
	spark::io::BufferAdaptor adaptor(buffer);
	auto ptr = adaptor.read_ptr();
	ASSERT_EQ(*ptr, buffer[0]);
	adaptor.skip(1);
	ptr = adaptor.read_ptr();
	ASSERT_EQ(*ptr, buffer[1]);
	adaptor.skip(1);
	ptr = adaptor.read_ptr();
	ASSERT_EQ(*ptr, buffer[2]);
}

TEST(BufferAdaptor, Subscript) {
	std::array<std::uint8_t, 3> buffer { 1, 2, 3 };
	spark::io::BufferAdaptor adaptor(buffer);
	ASSERT_EQ(adaptor[0], 1);
	ASSERT_EQ(adaptor[1], 2);
	ASSERT_EQ(adaptor[2], 3);
	buffer[0] = 4;
	ASSERT_EQ(adaptor[0], 4);
	adaptor[0] = 5;
	ASSERT_EQ(adaptor[0], 5);
	ASSERT_EQ(adaptor[0], buffer[0]);
}

TEST(BufferAdaptor, FindFirstOf) {
	std::vector<char> buffer;
	spark::io::BufferAdaptor adaptor(buffer);
	const auto str = "The quick brown fox jumped over the lazy dog"sv;
	adaptor.write(str.data(), str.size());
	auto pos = adaptor.find_first_of('\0');
	ASSERT_EQ(pos, adaptor.npos); // direct string write is not terminated
	pos = adaptor.find_first_of('g');
	ASSERT_EQ(pos, 43);
	pos = adaptor.find_first_of('T');
	ASSERT_EQ(pos, 0);
	pos = adaptor.find_first_of('t');
	ASSERT_EQ(pos, 32);
}

// test optimised write() for buffers supporting resize_and_overwrite
TEST(BufferAdaptor, StringBuffer) {
	std::string buffer;
	spark::io::BufferAdaptor adaptor(buffer);
	const auto str = "The quick brown fox jumped over the lazy dog"sv;
	adaptor.write(str.data(), str.size());
	ASSERT_EQ(buffer, str);
}