/*
 * Copyright (c) 2018 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <spark/buffers/pmr/BinaryStream.h>
#include <spark/buffers/pmr/BufferAdaptor.h>
#include <spark/buffers/DynamicBuffer.h>
#include <gtest/gtest.h>
#include <gsl/gsl_util>
#include <algorithm>
#include <array>
#include <chrono>
#include <numeric>
#include <random>
#include <cstdint>
#include <cstdlib>

using namespace ember;

TEST(BinaryStreamPMR, MessageReadLimit) {
	std::array<std::uint8_t, 14> ping {
		0x00, 0x0C, 0xDC, 0x01, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0xF4, 0x01, 0x00, 0x00
	};

	// write the ping packet data twice to the buffer
	spark::io::DynamicBuffer<32> buffer;
	buffer.write(ping.data(), ping.size());
	buffer.write(ping.data(), ping.size());

	// read one packet back out (reuse the ping array)
	spark::io::pmr::BinaryStream stream(buffer, ping.size());
	ASSERT_EQ(stream.read_limit(), ping.size());
	ASSERT_NO_THROW(stream.get(ping.data(), ping.size()))
		<< "Failed to read packet back from stream";

	// attempt to read past the stream message bound
	ASSERT_THROW(stream.get(ping.data(), ping.size()), spark::io::stream_read_limit)
		<< "Message boundary was not respected";
	ASSERT_EQ(stream.state(), spark::io::StreamState::READ_LIMIT_ERR)
		<< "Unexpected stream state";
}

TEST(BinaryStreamPMR, BufferLimit) {
	std::array<std::uint8_t, 14> ping {
		0x00, 0x0C, 0xDC, 0x01, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0xF4, 0x01, 0x00, 0x00
	};

	// write the ping packet data to the buffer
	spark::io::DynamicBuffer<32> buffer;
	buffer.write(ping.data(), ping.size());

	// read all data back out
	spark::io::pmr::BinaryStream stream(buffer);
	ASSERT_NO_THROW(stream.get(ping.data(), ping.size()))
		<< "Failed to read packet back from stream";

	// attempt to read past the buffer bound
	ASSERT_THROW(stream.get(ping.data(), ping.size()), spark::io::buffer_underrun)
		<< "Message boundary was not respected";
	ASSERT_EQ(stream.state(), spark::io::StreamState::BUFF_LIMIT_ERR)
		<< "Unexpected stream state";
}

TEST(BinaryStreamPMR, ReadWriteInts) {
	spark::io::DynamicBuffer<32> buffer;
	spark::io::pmr::BinaryStream stream(buffer);

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

TEST(BinaryStreamPMR, ReadWriteStdString) {
	spark::io::DynamicBuffer<32> buffer;
	spark::io::pmr::BinaryStream stream(buffer);
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

TEST(BinaryStreamPMR, ReadWriteCString) {
	spark::io::DynamicBuffer<32> buffer;
	spark::io::pmr::BinaryStream stream(buffer);
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

TEST(BinaryStreamPMR, ReadWriteVector) {
	spark::io::DynamicBuffer<32> buffer;
	spark::io::pmr::BinaryStream stream(buffer);

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

TEST(BinaryStreamPMR, Clear) {
	spark::io::DynamicBuffer<32> buffer;
	spark::io::pmr::BinaryStream stream(buffer);
	stream << 0xBADF00D;

	ASSERT_TRUE(!stream.empty());
	ASSERT_TRUE(!buffer.empty());

	stream.skip(stream.size());

	ASSERT_TRUE(stream.empty());
	ASSERT_TRUE(buffer.empty());
}

TEST(BinaryStreamPMR, Skip) {
	spark::io::DynamicBuffer<32> buffer;
	spark::io::pmr::BinaryStream stream(buffer);

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

TEST(BinaryStreamPMR, CanWriteSeek) {
	spark::io::DynamicBuffer<32> buffer;
	spark::io::pmr::BinaryStream stream(buffer);
	ASSERT_EQ(buffer.can_write_seek(), stream.can_write_seek());
}

TEST(BinaryStreamPMR, GetPut) {
	spark::io::DynamicBuffer<32> buffer;
	spark::io::pmr::BinaryStream stream(buffer);
	std::vector<std::uint8_t> in { 1, 2, 3, 4, 5 };
	std::vector<std::uint8_t> out(in.size());

	stream.put(in.data(), in.size());
	stream.get(out.data(), out.size());

	ASSERT_EQ(stream.total_read(), out.size());
	ASSERT_EQ(stream.total_write(), in.size());
	ASSERT_EQ(in, out);
}

TEST(BinaryStreamPMR, Fill) {
	std::vector<std::uint8_t> buffer;
	spark::io::pmr::BufferAdaptor adaptor(buffer);
	spark::io::pmr::BinaryStream stream(adaptor);
	stream.fill<30>(128);
	ASSERT_EQ(buffer.size(), 30);
	ASSERT_EQ(stream.total_write(), 30);
	stream.fill<2>(128);
	ASSERT_EQ(buffer.size(), 32);
	ASSERT_EQ(stream.total_write(), 32);
	auto it = std::ranges::find_if(buffer,  [](auto i) { return i != 128; });
	ASSERT_EQ(it, buffer.end());
}

TEST(BinaryStreamPMR, Array) {
	std::vector<char> buffer;
	spark::io::pmr::BufferAdaptor adaptor(buffer);
	spark::io::pmr::BinaryStream stream(adaptor);
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

TEST(BinaryStreamPMR, PutIntegralLiterals) {
	spark::io::DynamicBuffer<64> buffer;
	spark::io::pmr::BinaryStream stream(buffer);
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

TEST(BinaryStreamPMR, StringViewWrite) {
	std::string buffer;
	spark::io::pmr::BufferAdaptor adaptor(buffer);
	spark::io::pmr::BinaryStream stream(adaptor);
	std::string_view view { "There's coffee in that nebula" };
	stream << view;
	std::string res;
	stream >> res;
	ASSERT_EQ(view, res);
}

TEST(BinaryStreamPMR, CStringViewWrite) {
	std::string buffer;
	spark::io::pmr::BufferAdaptor adaptor(buffer);
	spark::io::pmr::BinaryStream stream(adaptor);
	ember::cstring_view view { "There's coffee in that nebula" };
	stream << view;
	std::string res;
	stream >> res;
	ASSERT_EQ(view, res);
}