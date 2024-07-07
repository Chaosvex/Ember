/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <spark/buffers/DynamicBuffer.h>
#define BUFFER_SEQUENCE_DEBUG
#include <spark/buffers/BufferSequence.h>
#undef BUFFER_SEQUENCE_DEBUG
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <cstring>

namespace spark = ember::spark;

TEST(DynamicBuffer, Size) {
	spark::DynamicBuffer<32> chain;
	ASSERT_EQ(0, chain.size()) << "Chain size is incorrect";

	chain.reserve(50);
	ASSERT_EQ(50, chain.size()) << "Chain size is incorrect";

	char buffer[20];
	chain.read(buffer, sizeof(buffer));
	ASSERT_EQ(30, chain.size()) << "Chain size is incorrect";

	chain.skip(10);
	ASSERT_EQ(20, chain.size()) << "Chain size is incorrect";

	chain.write(buffer, 20);
	ASSERT_EQ(40, chain.size()) << "Chain size is incorrect";

	chain.clear();
	ASSERT_EQ(0, chain.size()) << "Chain size is incorrect";
}

TEST(DynamicBuffer, ReadWriteConsistency) {
	spark::DynamicBuffer<32> chain;
	char text[] = "The quick brown fox jumps over the lazy dog";
	int num = 41521;

	chain.write(text, sizeof(text));
	chain.write(&num, sizeof(int));

	char text_out[sizeof(text)];
	int num_out;

	chain.read(text_out, sizeof(text));
	chain.read(&num_out, sizeof(int));

	ASSERT_EQ(num, num_out) << "Read produced incorrect result";
	ASSERT_STREQ(text, text_out) << "Read produced incorrect result";
	ASSERT_EQ(0, chain.size()) << "Chain should be empty";
}

TEST(DynamicBuffer, ReserveFetchConsistency) {
	spark::DynamicBuffer<32> chain;
	char text[] = "The quick brown fox jumps over the lazy dog";
	const std::size_t text_len = sizeof(text);

	chain.reserve(text_len);
	ASSERT_EQ(text_len, chain.size()) << "Chain size is incorrect";

	auto buffers = chain.fetch_buffers(text_len);
	std::string output; output.resize(text_len);

	// store text in the retrieved buffers
	std::size_t offset = 0;

	for(auto& buffer : buffers) {
		std::memcpy(const_cast<std::byte*>(buffer->read_data()), text + offset, buffer->size());
		offset += buffer->size();

		if(offset > text_len || !offset) {
			break;
		}
	}

	// read text back out
	chain.read(&output[0], text_len);

	ASSERT_EQ(0, chain.size()) << "Chain size is incorrect";
	ASSERT_STREQ(text, output.c_str()) << "Read produced incorrect result";
}

TEST(DynamicBuffer, Skip) {
	spark::DynamicBuffer<32> chain;
	int foo = 960;
	int bar = 296;

	chain.write(&foo, sizeof(int));
	chain.write(&bar, sizeof(int));
	chain.skip(sizeof(int));
	chain.read(&foo, sizeof(int));

	ASSERT_EQ(0, chain.size()) << "Chain size is incorrect";
	ASSERT_EQ(bar, foo) << "Skip produced incorrect result";
}

TEST(DynamicBuffer, Clear) {
	spark::DynamicBuffer<32> chain;
	const int iterations = 100;

	for(int i = 0; i < iterations; ++i) {
		chain.write(&i, sizeof(int));
	}

	ASSERT_EQ(sizeof(int) * iterations, chain.size()) << "Chain size is incorrect";
	chain.clear();
	ASSERT_EQ(0, chain.size()) << "Chain size is incorrect";
	ASSERT_TRUE(chain.empty());
	ASSERT_EQ(0, chain.block_count());
}

TEST(DynamicBuffer, AttachTail) {
	spark::DynamicBuffer<32> chain;
	auto buffer = chain.allocate();

	std::string text("This is a string that is almost certainly longer than 32 bytes");
	std::size_t written = buffer->write(text.c_str(), text.length());

	ASSERT_EQ(0, chain.size()) << "Chain size is incorrect";
	chain.push_back(buffer);
	chain.skip(32); // skip first block
	chain.advance_write_cursor(written);
	ASSERT_EQ(written, chain.size()) << "Chain size is incorrect";

	std::string output; output.resize(written);

	chain.read(&output[0], written);
	ASSERT_EQ(0, chain.size()) << "Chain size is incorrect";
	ASSERT_EQ(0, std::memcmp(text.data(), output.data(), written)) << "Output is incorrect";
}

TEST(DynamicBuffer, PopFrontPushBack) {
	spark::DynamicBuffer<32> chain;
	auto buffer = chain.allocate();

	std::string text("This is a string that is almost certainly longer than 32 bytes");
	std::size_t written = buffer->write(text.c_str(), text.length());

	ASSERT_EQ(0, chain.size()) << "Chain size is incorrect";
	chain.push_back(buffer);
	ASSERT_EQ(written, chain.size()) << "Chain size is incorrect";
	auto front = chain.pop_front();
	chain.deallocate(front);
	ASSERT_EQ(0, chain.size()) << "Chain size is incorrect";
}

TEST(DynamicBuffer, RetrieveTail) {
	spark::DynamicBuffer<32> chain;
	std::string text("This string is < 32 bytes"); // could this fail on exotic platforms?
	chain.write(text.data(), text.length());

	auto tail = chain.back();
	ASSERT_EQ(0, std::memcmp(text.data(), tail->storage.data(), text.length())) << "Tail data is incorrect";
}

TEST(DynamicBuffer, Copy) {
	spark::DynamicBuffer<32> chain;
	int output, foo = 54543;
	chain.write(&foo, sizeof(int));
	ASSERT_EQ(sizeof(int), chain.size());
	chain.copy(&output, sizeof(int));
	ASSERT_EQ(sizeof(int), chain.size()) << "Chain size is incorrect";
	ASSERT_EQ(foo, output) << "Copy output is incorrect";
}

TEST(DynamicBuffer, CopyChain) {
	spark::DynamicBuffer<sizeof(int)> chain, chain2;
	int foo = 5491;
	int output;

	chain.write(&foo, sizeof(int));
	chain.write(&foo, sizeof(int));
	ASSERT_EQ(sizeof(int) * 2, chain.size()) << "Chain size is incorrect";
	ASSERT_EQ(0, chain2.size()) << "Chain size is incorrect";

	chain2 = chain;
	ASSERT_EQ(sizeof(int) * 2, chain.size()) << "Chain size is incorrect";
	ASSERT_EQ(sizeof(int) * 2, chain2.size()) << "Chain size is incorrect";

	chain.read(&output, sizeof(int));
	ASSERT_EQ(sizeof(int), chain.size()) << "Chain size is incorrect";
	ASSERT_EQ(sizeof(int) * 2, chain2.size()) << "Chain size is incorrect";

	chain.clear();
	ASSERT_EQ(0, chain.size()) << "Chain size is incorrect";
	ASSERT_EQ(sizeof(int) * 2, chain2.size()) << "Chain size is incorrect";

	chain2.read(&output, sizeof(int));
	ASSERT_EQ(foo, output) << "Chain output is incorrect";
}

TEST(DynamicBuffer, MoveChain) {
	spark::DynamicBuffer<32> chain, chain2;
	int foo = 23113;

	chain.write(&foo, sizeof(int));
	ASSERT_EQ(sizeof(int), chain.size()) << "Chain size is incorrect";
	ASSERT_EQ(0, chain2.size()) << "Chain size is incorrect";

	chain2 = std::move(chain);
	ASSERT_EQ(0, chain.size()) << "Chain size is incorrect";
	ASSERT_EQ(sizeof(int), chain2.size()) << "Chain size is incorrect";

	int output;
	chain2.read(&output, sizeof(int));
	ASSERT_EQ(foo, output) << "Chain output is incorrect";
}

TEST(DynamicBuffer, WriteSeek) {
	spark::DynamicBuffer<1> chain; // ensure the data is split over multiple buffer nodes
	const std::array<std::uint8_t, 6> data {0x00, 0x01, 0x00, 0x00, 0x04, 0x05};
	const std::array<std::uint8_t, 2> seek_data {0x02, 0x03};
	const std::array<std::uint8_t, 4> expected_data {0x00, 0x01, 0x02, 0x03};

	chain.write(data.data(), data.size());
	chain.write_seek(spark::BufferSeek::SK_BACKWARD, 4);
	chain.write(seek_data.data(), seek_data.size());

	// make sure the chain is four bytes (6 written, (-)4 rewound, (+)2 rewritten = 4)
	ASSERT_EQ(chain.size(), 4) << "Chain size is incorrect";

	std::vector<std::uint8_t> out(chain.size());
	chain.copy(out.data(), out.size());

	ASSERT_EQ(0, std::memcmp(out.data(), expected_data.data(), expected_data.size()))
		<< "Buffer contains incorrect data pattern";

	// put the write cursor back to its original position and write more data
	chain.write_seek(spark::BufferSeek::SK_FORWARD, 2);

	// should be six bytes in there
	ASSERT_EQ(chain.size(), data.size()) << "Chain size is incorrect";

	const std::array<std::uint8_t, 8> final_data {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
	const std::array<std::uint8_t, 2> new_data {0x06, 0x07};
	chain.write(new_data.data(), new_data.size());

	// should be eight bytes in there
	ASSERT_EQ(chain.size(), final_data.size()) << "Chain size is incorrect";

	// check the pattern/sequence/whatever
	out.resize(chain.size());
	chain.read(out.data(), out.size());

	ASSERT_EQ(0, std::memcmp(out.data(), final_data.data(), final_data.size()))
		<< "Buffer contains incorrect data pattern";
}

TEST(DynamicBuffer, ReadIterator) {
	spark::DynamicBuffer<16> chain; // ensure the string is split over multiple buffers
	spark::BufferSequence<16> sequence(chain);
	std::string skip("Skipping");
	std::string input("The quick brown fox jumps over the lazy dog");
	std::string output;

	chain.write(skip.data(), skip.size());
	chain.write(input.data(), input.size());
	chain.skip(skip.size()); // ensure skipped data isn't read back out
	
	for(auto i = sequence.begin(), j = sequence.end(); i != j; ++i) {
		auto buffer = i.get_buffer();
		std::copy(buffer.first, buffer.first + buffer.second, std::back_inserter(output));
	}

	ASSERT_EQ(input, output) << "Read iterator produced incorrect result";
}

TEST(DynamicBuffer, ASIOIteratorRegressionTest) {
	spark::DynamicBuffer<1> chain;
	spark::BufferSequence<1> sequence(chain);

	// 119 bytes (size of 1.12.1 LoginChallenge packet)
	std::string input("Lorem ipsum dolor sit amet, consectetur adipiscing elit."
	                  " Etiam sagittis pulvinar massa nec pellentesque. Integer metus.");

	chain.write(input.data(), input.length());
	ASSERT_EQ(119, chain.size()) << "Chain size was incorrect";

	auto it = sequence.begin();
	std::size_t bytes_sent = 0;

	// do first read
	for(std::size_t i = 0; it != sequence.end() && i != 64; ++i, ++it) {
		bytes_sent += it.get_buffer().second;
	}

	chain.skip(bytes_sent);
	ASSERT_EQ(64, bytes_sent) << "First read length was incorrect";
	ASSERT_EQ(55, chain.size()) << "Chain size was incorrect";

	auto it_s = sequence.begin();
	bytes_sent = 0;

	// do second read
	for(std::size_t i = 0; it_s != sequence.end() && i != 64; ++i, ++it_s) {
		bytes_sent += it_s.get_buffer().second;
	}

	chain.skip(bytes_sent);
	ASSERT_EQ(55, bytes_sent) << "Second read length was incorrect";
	ASSERT_EQ(0, chain.size()) << "Chain size was incorrect";

	//chain.clear();
	input = "xy"; // two bytes - size of LoginProof if failed login
	chain.write(input.data(), input.length());
	ASSERT_EQ(2, chain.size()) << "Chain size was incorrect";

	auto it_t = sequence.begin();
	bytes_sent = 0;

	// do third read
	for(std::size_t i = 0; it_t != sequence.end() && i != 64; ++i, ++it_t) {
		bytes_sent += it_t.get_buffer().second;
	}

	chain.skip(bytes_sent);
	ASSERT_EQ(2, bytes_sent) << "Regression found - read length was incorrect";
	ASSERT_EQ(0, chain.size()) << "Chain size was incorrect";
}

TEST(DynamicBuffer, Empty) {
	spark::DynamicBuffer<32> chain;
	ASSERT_TRUE(chain.empty());
	const auto value = 0;
	chain.write(&value, sizeof(value));
	ASSERT_FALSE(chain.empty());
}

TEST(DynamicBuffer, BlockSize) {
	spark::DynamicBuffer<32> chain;
	ASSERT_EQ(chain.block_size(), 32);
	spark::DynamicBuffer<64> chain2;
	ASSERT_EQ(chain2.block_size(), 64);
}

TEST(DynamicBuffer, BlockCount) {
	spark::DynamicBuffer<1> chain;
	const std::uint8_t value = 0;
	chain.write(&value, sizeof(value));
	ASSERT_EQ(chain.block_count(), 1);
	chain.write(&value, sizeof(value));
	ASSERT_EQ(chain.block_count(), 2);
	chain.write(&value, sizeof(value));
	chain.write(&value, sizeof(value));
	ASSERT_EQ(chain.block_count(), 4);
	auto node = chain.pop_front();
	chain.deallocate(node);
	ASSERT_EQ(chain.block_count(), 3);
}