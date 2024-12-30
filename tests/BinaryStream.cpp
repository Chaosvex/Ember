/*
 * Copyright (c) 2018 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <spark/buffers/DynamicBuffer.h>
#include <spark/buffers/StaticBuffer.h>
#include <spark/buffers/BinaryStream.h>
#include <spark/buffers/BufferAdaptor.h>
#include <shared/utility/cstring_view.hpp>
#include <gtest/gtest.h>
#include <gsl/gsl_util>
#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <numeric>
#include <random>
#include <cstdint>
#include <cstdlib>
#include <cstring>

using namespace ember;

TEST(BinaryStream, MessageReadLimit) {
	std::array<std::uint8_t, 14> ping {
		0x00, 0x0C, 0xDC, 0x01, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0xF4, 0x01, 0x00, 0x00
	};

	// write the ping packet data twice to the buffer
	spark::io::DynamicBuffer<32> buffer;
	buffer.write(ping.data(), ping.size());
	buffer.write(ping.data(), ping.size());

	// read one packet back out (reuse the ping array)
	spark::io::BinaryStream stream(buffer, ping.size());
	ASSERT_EQ(stream.read_limit(), ping.size());
	ASSERT_NO_THROW(stream.get(ping.data(), ping.size()))
		<< "Failed to read packet back from stream";

	// attempt to read past the stream message bound
	ASSERT_THROW(stream.get(ping.data(), ping.size()), spark::io::stream_read_limit)
		<< "Message boundary was not respected";
	ASSERT_EQ(stream.state(), spark::io::StreamState::READ_LIMIT_ERR)
		<< "Unexpected stream state";
}

TEST(BinaryStream, BufferLimit) {
	std::array<std::uint8_t, 14> ping {
		0x00, 0x0C, 0xDC, 0x01, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0xF4, 0x01, 0x00, 0x00
	};

	// write the ping packet data to the buffer
	spark::io::DynamicBuffer<32> buffer;
	buffer.write(ping.data(), ping.size());

	// read all data back out
	spark::io::BinaryStream stream(buffer);
	ASSERT_NO_THROW(stream.get(ping.data(), ping.size()))
		<< "Failed to read packet back from stream";

	// attempt to read past the buffer bound
	ASSERT_THROW(stream.get(ping.data(), ping.size()), spark::io::buffer_underrun)
		<< "Message boundary was not respected";
	ASSERT_EQ(stream.state(), spark::io::StreamState::BUFF_LIMIT_ERR)
		<< "Unexpected stream state";
}

TEST(BinaryStream, ReadWriteInts) {
	spark::io::DynamicBuffer<32> buffer;
	spark::io::BinaryStream stream(buffer);

	const std::uint16_t in { 100 };
	stream << in;

	ASSERT_EQ(stream.size(), sizeof(in));
	ASSERT_EQ(stream.size(), buffer.size());

	std::uint16_t out;
	stream >> out;

	ASSERT_EQ(in, out);
	ASSERT_TRUE(stream.empty());
	ASSERT_TRUE(buffer.empty());
	ASSERT_EQ(stream.state(), spark::io::StreamState::OK)
		<< "Unexpected stream state";
}

TEST(BinaryStream, ReadWriteStdString) {
	spark::io::DynamicBuffer<32> buffer;
	spark::io::BinaryStream stream(buffer);
	const std::string in { "The quick brown fox jumped over the lazy dog" };
	stream << in;

	// +1 to account for the terminator that's written
	ASSERT_EQ(stream.size(), in.size() + 1);

	std::string out;
	stream >> out;

	ASSERT_TRUE(stream.empty());
	ASSERT_EQ(in, out);
	ASSERT_EQ(stream.state(), spark::io::StreamState::OK)
		<< "Unexpected stream state";
}

TEST(BinaryStream, ReadWriteCString) {
	spark::io::DynamicBuffer<32> buffer;
	spark::io::BinaryStream stream(buffer);
	const char* in { "The quick brown fox jumped over the lazy dog" };
	stream << in;

	ASSERT_EQ(stream.size(), strlen(in) + 1);

	std::string out;
	stream >> out;

	ASSERT_TRUE(stream.empty());
	ASSERT_EQ(0, strcmp(in, out.c_str()));
	ASSERT_EQ(stream.state(), spark::io::StreamState::OK)
		<< "Unexpected stream state";
}

TEST(BinaryStream, ReadWriteVector) {
	spark::io::DynamicBuffer<32> buffer;
	spark::io::BinaryStream stream(buffer);

	const auto time = std::chrono::system_clock::now().time_since_epoch();
	const unsigned int seed = gsl::narrow_cast<unsigned int>(time.count());
	std::srand(seed);

	std::vector<int> in(std::rand() % 200);
	std::ranges::iota(in, std::rand() % 100);
	std::ranges::shuffle(in, std::default_random_engine(seed));

	stream.put(in.begin(), in.end());

	ASSERT_EQ(stream.size(), in.size() * sizeof(int));

	// read the integers back one by one, making sure they
	// match the expected value
	for(auto it = in.begin(); it != in.end(); ++it) {
		int output;
		stream >> output;
		ASSERT_EQ(output, *it);
	}

	stream.put(in.begin(), in.end());
	std::vector<int> out(in.size());

	// read the integers to an output buffer and compare both
	stream.get(out.begin(), out.end());
	ASSERT_EQ(in, out);
	ASSERT_EQ(stream.state(), spark::io::StreamState::OK)
		<< "Unexpected stream state";
}

TEST(BinaryStream, Clear) {
	spark::io::DynamicBuffer<32> buffer;
	spark::io::BinaryStream stream(buffer);
	stream << 0xBADF00D;

	ASSERT_TRUE(!stream.empty());
	ASSERT_TRUE(!buffer.empty());

	stream.skip(stream.size());

	ASSERT_TRUE(stream.empty());
	ASSERT_TRUE(buffer.empty());
}

TEST(BinaryStream, Skip) {
	spark::io::DynamicBuffer<32> buffer;
	spark::io::BinaryStream stream(buffer);

	const std::uint64_t in {0xBADF00D};
	stream << in << in;
	stream.skip(sizeof(in));

	ASSERT_EQ(stream.size(), sizeof(in));
	ASSERT_EQ(stream.size(), buffer.size());

	std::uint64_t out;
	stream >> out;

	ASSERT_TRUE(stream.empty());
	ASSERT_EQ(in, out);
}

TEST(BinaryStream, CanWriteSeek) {
	spark::io::DynamicBuffer<32> buffer;
	spark::io::BinaryStream stream(buffer);
	ASSERT_EQ(buffer.can_write_seek(), stream.can_write_seek());
}

TEST(BinaryStream, GetPut) {
	spark::io::DynamicBuffer<32> buffer;
	spark::io::BinaryStream stream(buffer);
	std::vector<std::uint8_t> in { 1, 2, 3, 4, 5 };
	std::vector<std::uint8_t> out(in.size());

	stream.put(in.data(), in.size());
	stream.get(out.data(), out.size());

	ASSERT_EQ(stream.total_read(), out.size());
	ASSERT_EQ(stream.total_write(), in.size());
	ASSERT_EQ(in, out);
}

TEST(BinaryStream, Fill) {
	std::vector<std::uint8_t> buffer;
	spark::io::BufferAdaptor adaptor(buffer);
	spark::io::BinaryStream stream(adaptor);
	stream.fill<30>(128);
	ASSERT_EQ(buffer.size(), 30);
	ASSERT_EQ(stream.total_write(), 30);
	stream.fill<2>(128);
	ASSERT_EQ(buffer.size(), 32);
	ASSERT_EQ(stream.total_write(), 32);
	auto it = std::ranges::find_if(buffer,  [](auto i) { return i != 128; });
	ASSERT_EQ(it, buffer.end());
}

TEST(BinaryStream, NoCopyStringRead) {
	std::vector<char> buffer;
	spark::io::BufferAdaptor adaptor(buffer);
	spark::io::BinaryStream stream(adaptor);
	const std::string input { "The quick brown fox jumped over the lazy dog" };
	const std::uint32_t trailing { 0x0DDBA11 };
	stream << input << trailing;

	// check this stream uses a contiguous buffer
	const auto contig = std::is_same<decltype(stream)::contiguous_type, spark::io::is_contiguous>::value;
	ASSERT_TRUE(contig);

	// find the end of the string within the buffer
	const auto stream_buf = stream.buffer();
	const auto pos = stream_buf->find_first_of('\0');
	ASSERT_NE(pos, adaptor.npos);

	// create a view into the buffer & skip ahead so the next read continues as normal
	std::string_view output(stream_buf->read_ptr(), pos);
	ASSERT_EQ(input, output);

	// ensure we can still read subsequent data as normal
	stream.skip(pos + 1); // +1 to skip terminator
	std::uint32_t trailing_output = 0;
	stream >> trailing_output ;
	ASSERT_EQ(trailing, trailing_output);
}

TEST(BinaryStream, StringViewRead) {
	std::vector<char> buffer;
	spark::io::BufferAdaptor adaptor(buffer);
	spark::io::BinaryStream stream(adaptor);
	const std::string input { "The quick brown fox jumped over the lazy dog" };
	const std::uint32_t trailing { 0x0DDBA11 };
	stream << input << trailing;

	auto view = stream.view();
	ASSERT_EQ(input, view);

	// ensure we can still read subsequent data as normal
	std::uint32_t trailing_output = 0;
	stream >> trailing_output;
	ASSERT_EQ(trailing, trailing_output);
}

TEST(BinaryStream, PartialStringViewRead) {
	std::vector<char> buffer;
	spark::io::BufferAdaptor adaptor(buffer);
	spark::io::BinaryStream stream(adaptor);
	const std::string input { "The quick brown fox jumped over the lazy dog" };
	stream << input;

	auto span = stream.span(20);
	std::string_view view(span);
	ASSERT_EQ("The quick brown fox ", view);

	// read the rest of the string
	view = stream.view();
	ASSERT_EQ("jumped over the lazy dog", view);
	ASSERT_TRUE(stream.empty());
}

TEST(BinaryStream, StringViewStream) {
	std::vector<char> buffer;
	spark::io::BufferAdaptor adaptor(buffer);
	spark::io::BinaryStream stream(adaptor);
	const std::string input { "The quick brown fox jumped over the lazy dog" };
	const std::uint32_t trailing { 0xDEFECA7E };
	stream << input << trailing;

	std::string_view output;
	stream >> output;
	ASSERT_EQ(input, output);

	// ensure we can still read subsequent data as normal
	std::uint32_t trailing_output = 0;
	stream >> trailing_output;
	ASSERT_EQ(trailing, trailing_output);
	
	// make a sly modification to the buffer and check the string_view matches
	ASSERT_FALSE(buffer.empty());
	buffer[0] = 'A';
	ASSERT_EQ(buffer[0], output[0]);
	ASSERT_NE(input, output);
}

TEST(BinaryStream, Array) {
	std::vector<char> buffer;
	spark::io::BufferAdaptor adaptor(buffer);
	spark::io::BinaryStream stream(adaptor);
	const int arr[] = { 1, 2, 3 };
	stream << arr;
	int val = 0;
	stream >> val;
	ASSERT_EQ(val, 1);
	stream >> val;
	ASSERT_EQ(val, 2);
	stream >> val;
	ASSERT_EQ(val, 3);
}

TEST(BinaryStream, Span) {
	std::vector<char> buffer;
	spark::io::BufferAdaptor adaptor(buffer);
	spark::io::BinaryStream stream(adaptor);
	const int arr[] = { 4, 9, 2, 1 }; // chosen by fair dice roll
	stream << arr;
	auto span = stream.span<int>(4);
	ASSERT_TRUE(stream.empty());
	// not checking directly against the array in case it somehow gets clobbered,
	// which would mean the span would get clobbered and succeed where it shouldn't
	ASSERT_EQ(span[0], 4);
	ASSERT_EQ(span[1], 9);
	ASSERT_EQ(span[2], 2);
	ASSERT_EQ(span[3], 1);
}

TEST(BinaryStream, CStringView) {
	std::vector<char> buffer;
	spark::io::BufferAdaptor adaptor(buffer);
	spark::io::BinaryStream stream(adaptor);
	std::string_view view { "There's coffee in that nebula" };
	stream << view;
	ember::cstring_view cview;
	stream >> cview;
	ASSERT_EQ(view, cview);
	const auto len = std::strlen(cview.c_str());
	ASSERT_EQ(view.size(), len);
	ASSERT_EQ(*(cview.data() + len), '\0');
}

TEST(BinaryStream, StaticBufferWrite) {
	spark::io::StaticBuffer<char, 4> buffer;
	spark::io::BinaryStream stream(buffer);
	stream << 'a' << 'b' << 'c' << 'd';
	ASSERT_EQ(buffer[0], 'a');
	ASSERT_EQ(buffer[1], 'b');
	ASSERT_EQ(buffer[2], 'c');
	ASSERT_EQ(buffer[3], 'd');
	ASSERT_TRUE(stream);
}

TEST(BinaryStream, StaticBufferDirectWrite) {
	spark::io::StaticBuffer<char, 4> buffer;
	spark::io::BinaryStream stream(buffer);
	std::uint32_t input = 0xBEE5BEE5;
	std::uint32_t output = 0;
	buffer.write(&input, sizeof(input));
	stream >> output;
	ASSERT_EQ(input, output);
	ASSERT_TRUE(stream);
}

TEST(BinaryStream, StaticBufferOverflow) {
	spark::io::StaticBuffer<char, 4> buffer;
	spark::io::BinaryStream stream(buffer);
	ASSERT_THROW(stream << std::uint64_t(1), spark::io::buffer_overflow);
	ASSERT_TRUE(stream);
}

TEST(BinaryStream, StaticBufferRead) {
	spark::io::StaticBuffer<char, 4> buffer;
	const std::uint32_t input = 0x11223344;
	buffer.write(&input, sizeof(input));
	spark::io::BinaryStream stream(buffer);
	std::uint32_t output = 0;
	stream >> output;
	ASSERT_EQ(input, output);
	ASSERT_TRUE(stream);
}

TEST(BinaryStream, StaticBufferUnderrun) {
	spark::io::StaticBuffer<char, 4> buffer;
	spark::io::BinaryStream stream(buffer);
	std::uint32_t input = 0xBEEFBEEF;
	std::uint32_t output = 0;
	stream << input;
	stream >> output;
	ASSERT_THROW(stream >> output, spark::io::buffer_underrun);
	ASSERT_FALSE(stream);
	ASSERT_EQ(input, output);
}

TEST(BinaryStream, StaticBufferUnderrunNoExcept) {
	spark::io::StaticBuffer<char, 4> buffer;
	spark::io::BinaryStream<decltype(buffer), spark::io::no_throw> stream(buffer);
	std::uint32_t output = 0;
	stream << output;
	stream >> output;
	ASSERT_NO_THROW(stream >> output);
	ASSERT_FALSE(stream);
	ASSERT_EQ(output, 0);
}

TEST(BinaryStream, PutIntegralLiterals) {
	spark::io::StaticBuffer<char, 64> buffer;
	spark::io::BinaryStream stream(buffer);
	stream.put<std::uint64_t>(std::numeric_limits<std::uint64_t>::max());
	stream.put<std::uint32_t>(std::numeric_limits<std::uint32_t>::max());
	stream.put<std::uint16_t>(std::numeric_limits<std::uint16_t>::max());
	stream.put<std::uint8_t>(std::numeric_limits<std::uint8_t>::max());
	stream.put<std::int64_t>(std::numeric_limits<std::int64_t>::max());
	stream.put<std::int32_t>(std::numeric_limits<std::int32_t>::max());
	stream.put<std::int16_t>(std::numeric_limits<std::int16_t>::max());
	stream.put<std::int8_t>(std::numeric_limits<std::int8_t>::max());
	stream.put(1.5f);
	stream.put(3.0);
	std::uint64_t resultu64 = 0;
	std::uint32_t resultu32 = 0;
	std::uint16_t resultu16 = 0;
	std::uint8_t resultu8 = 0;
	std::int64_t result64 = 0;
	std::int32_t result32 = 0;
	std::int16_t result16 = 0;
	std::int8_t result8 = 0;
	float resultf = 0.0f;
	double resultd = 0.0;
	stream >> resultu64 >> resultu32 >> resultu16 >> resultu8;
	stream >> result64 >> result32 >> result16 >> result8;
	stream >> resultf >> resultd;
	ASSERT_FLOAT_EQ(1.5f, resultf);
	ASSERT_DOUBLE_EQ(3.0, resultd);
	ASSERT_EQ(resultu8, std::numeric_limits<std::uint8_t>::max());
	ASSERT_EQ(resultu16, std::numeric_limits<std::uint16_t>::max());
	ASSERT_EQ(resultu32, std::numeric_limits<std::uint32_t>::max());
	ASSERT_EQ(resultu64, std::numeric_limits<std::uint64_t>::max());
	ASSERT_EQ(result8, std::numeric_limits<std::int8_t>::max());
	ASSERT_EQ(result16, std::numeric_limits<std::int16_t>::max());
	ASSERT_EQ(result32, std::numeric_limits<std::int32_t>::max());
	ASSERT_EQ(result64, std::numeric_limits<std::int64_t>::max());
	ASSERT_TRUE(stream);
}