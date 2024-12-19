/*
 * Copyright (c) 2015 - 2020 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <spark/buffers/detail/IntrusiveStorage.h>
#include <gtest/gtest.h>
#include <array>
#include <string_view>

namespace spark = ember::spark;

TEST(IntrusiveStorage, Size) {
	const int iterations = 5;
	spark::io::detail::IntrusiveStorage<sizeof(int) * iterations> buffer;
	int foo = 24221;
	std::size_t written = 0;

	for(int i = 0; i < 5; ++i) {
		written += buffer.write(&foo, sizeof(int));
	}

	ASSERT_EQ(sizeof(int) * iterations, written) << "Number of bytes written is incorrect";
	ASSERT_EQ(sizeof(int) * iterations, buffer.size()) << "Buffer size is incorrect";

	// attempt to exceed capacity - write should return 0
	written = buffer.write(&foo, sizeof(int));
	ASSERT_EQ(0, written) << "Number of bytes written is incorrect";
	ASSERT_EQ(sizeof(int) * iterations, buffer.size()) << "Buffer size is incorrect";
}

TEST(IntrusiveStorage, ReadWriteConsistency) {
	const char text[] = "The quick brown fox jumps over the lazy dog";
	spark::io::detail::IntrusiveStorage<sizeof(text)> buffer;

	std::size_t written = buffer.write(text, sizeof(text));
	ASSERT_EQ(sizeof(text), written) << "Incorrect write size";

	char text_out[sizeof(text)];

	std::size_t read = buffer.read(text_out, sizeof(text));
	ASSERT_EQ(sizeof(text), read) << "Incorrect read size";

	ASSERT_STREQ(text, text_out) << "Read produced incorrect result";
	ASSERT_EQ(0, buffer.size()) << "Buffer should be empty";
}

TEST(IntrusiveStorage, Skip) {
	const char text[] = "The quick brown fox jumps over the lazy dog";
	spark::io::detail::IntrusiveStorage<sizeof(text)> buffer;

	buffer.write(text, sizeof(text));

	std::array<char, sizeof(text)> text_out{};
	std::size_t skipped = buffer.skip(4);
	ASSERT_EQ(4, skipped) << "Incorrect skip length";
	
	buffer.read(text_out.data(), sizeof(text) - 4);
	ASSERT_STREQ("quick brown fox jumps over the lazy dog", text_out.data())
		<< "Skip/read produced incorrect result";
}

TEST(IntrusiveStorage, ReadWriteStringView) {
	spark::io::detail::IntrusiveStorage<128, char> buffer;
	std::string_view str { "The quick brown fox jumped over the lazy dog" };
	buffer.write(str.data(), str.size()+1);
	ASSERT_EQ(str.size() + 1, buffer.size());
	ASSERT_STREQ(str.data(), buffer.read_data());

	std::array<char, 128> out{};
	buffer.read(out.data(), str.size() + 1);
	ASSERT_STREQ(str.data(), out.data());
	ASSERT_TRUE(buffer.size() == 0);
}

TEST(IntrusiveStorage, ReadWriteInts) {
	spark::io::detail::IntrusiveStorage<128, char> buffer;
	std::array<int, 4> in { 42, 1657, 1558, -1563 };
	buffer.write(in.data(), sizeof(in));
	ASSERT_EQ(sizeof(in), buffer.size());

	std::array<int, 4> out {};
	buffer.read(out.data(), sizeof(out));
	ASSERT_EQ(in, out);
	ASSERT_TRUE(buffer.size() == 0);
}

TEST(IntrusiveStorage, Subscript) {
	spark::io::detail::IntrusiveStorage<8, char> buffer;
	std::string_view str { "ABC" };
	buffer.write(str.data(), str.size()+1);
	ASSERT_EQ(str[0], buffer[0]);
	ASSERT_EQ(str[1], buffer[1]);
	ASSERT_EQ(str[2], buffer[2]);
	buffer[0] = 'C';
	buffer[1] = 'D';
	buffer[2] = 'E';
	ASSERT_EQ('C', buffer[0]);
	ASSERT_EQ('D', buffer[1]);
	ASSERT_EQ('E', buffer[2]);
}

TEST(IntrusiveStorage, AdvanceWrite) {
	spark::io::detail::IntrusiveStorage<32, char> buffer;
	constexpr std::string_view str {"A short string"};
	const std::size_t advance = 10;
	buffer.advance_write(advance);
	ASSERT_EQ(buffer.size(), advance);
	buffer.write(str.data(), str.size() + 1);
	ASSERT_EQ(buffer.size(), advance + str.size() + 1);
	std::array<char, str.size() + 1> out{};
	buffer.read(out.data(), advance); // skip the 'empty' data
	buffer.read(out.data(), str.size() + 1);
	ASSERT_STREQ(str.data(), out.data());
}
