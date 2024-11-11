/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#define _DEBUG_TLS_BLOCK_ALLOCATOR
#include <spark/buffers/allocators/TLSBlockAllocator.h>
#include <gsl/gsl_util>
#include <gtest/gtest.h>
#include <array>
#include <chrono>
#include <thread>
#include <cstdlib>

namespace spark = ember::spark;

TEST(TLSBlockAllocator, SingleAlloc) {
	spark::io::TLSBlockAllocator<std::uint64_t, 1> tlsalloc;
	auto mem = tlsalloc.allocate();
	ASSERT_EQ(tlsalloc.allocator.storage_active_count, 1);
	ASSERT_EQ(tlsalloc.allocator.new_active_count, 0);
	ASSERT_EQ(tlsalloc.total_allocs, 1);
	ASSERT_EQ(tlsalloc.total_deallocs, 0);
	tlsalloc.deallocate(mem);
	ASSERT_EQ(tlsalloc.allocator.storage_active_count, 0);
	ASSERT_EQ(tlsalloc.allocator.new_active_count, 0);
	ASSERT_EQ(tlsalloc.total_allocs, 1);
	ASSERT_EQ(tlsalloc.total_deallocs, 1);
}

TEST(TLSBlockAllocator, RandomAllocs) {
	const auto MAX_ALLOCS = 100u;
	spark::io::TLSBlockAllocator<std::uint64_t, MAX_ALLOCS> tlsalloc;
	std::array<std::uint64_t*, MAX_ALLOCS> chunks{};
	const auto time = std::chrono::system_clock::now().time_since_epoch();
	const unsigned int seed = gsl::narrow_cast<unsigned int>(time.count());
	std::srand(seed);
	const auto allocs = std::rand() % MAX_ALLOCS;
	const auto tls_total_alloc = tlsalloc.allocator.total_allocs;
	const auto tls_total_dealloc = tlsalloc.allocator.total_deallocs;

	for(std::size_t i = 0u; i < allocs; ++i) {
		auto mem = tlsalloc.allocate();
		chunks[i] = mem;
	}

	ASSERT_EQ(tlsalloc.total_allocs, allocs);
	ASSERT_EQ(tlsalloc.active_allocs, allocs);
	ASSERT_EQ(tlsalloc.total_deallocs, 0);
	ASSERT_EQ(tlsalloc.allocator.total_allocs, tls_total_alloc + allocs);
	ASSERT_EQ(tlsalloc.allocator.total_deallocs, tls_total_dealloc);

	for(std::size_t i = 0u; i < allocs; ++i) {
		tlsalloc.deallocate(chunks[i]);
	}

	ASSERT_EQ(tlsalloc.total_allocs, allocs);
	ASSERT_EQ(tlsalloc.active_allocs, 0);
	ASSERT_EQ(tlsalloc.total_deallocs, allocs);
	ASSERT_EQ(tlsalloc.allocator.total_allocs, tls_total_alloc + allocs);
	ASSERT_EQ(tlsalloc.allocator.total_deallocs, tls_total_dealloc + allocs);
}

TEST(TLSBlockAllocator, OverCapacity) {
	spark::io::TLSBlockAllocator<std::uint64_t, 1> tlsalloc;
	std::array<std::uint64_t*, 2> mem{};
	mem[0] = tlsalloc.allocate();
	mem[1] = tlsalloc.allocate();
	ASSERT_EQ(tlsalloc.allocator.storage_active_count, 1);
	ASSERT_EQ(tlsalloc.allocator.new_active_count, 1);
	ASSERT_EQ(tlsalloc.total_allocs, 2);
	ASSERT_EQ(tlsalloc.total_deallocs, 0);
	tlsalloc.deallocate(mem[0]);
	ASSERT_EQ(tlsalloc.allocator.storage_active_count, 0);
	ASSERT_EQ(tlsalloc.allocator.new_active_count, 1);
	ASSERT_EQ(tlsalloc.total_allocs, 2);
	ASSERT_EQ(tlsalloc.total_deallocs, 1);
	tlsalloc.deallocate(mem[1]);
	ASSERT_EQ(tlsalloc.allocator.storage_active_count, 0);
	ASSERT_EQ(tlsalloc.allocator.new_active_count, 0);
	ASSERT_EQ(tlsalloc.total_allocs, 2);
	ASSERT_EQ(tlsalloc.total_deallocs, 2);
}

TEST(TLSBlockAllocator, NoSharing) {
	spark::io::TLSBlockAllocator<std::uint64_t, 2> tlsalloc;
	const auto tls_total_alloc = tlsalloc.allocator.total_allocs;
	const auto tls_total_dealloc = tlsalloc.allocator.total_deallocs;
	auto chunk = tlsalloc.allocate();
	ASSERT_EQ(tlsalloc.allocator.storage_active_count, 1);
	ASSERT_EQ(tlsalloc.allocator.total_allocs, tls_total_alloc + 1);

	std::thread thread([&] {
		spark::io::TLSBlockAllocator<std::uint64_t, 2> tlsalloc;
		ASSERT_EQ(tlsalloc.allocator.total_allocs, 0);
		ASSERT_EQ(tlsalloc.allocator.storage_active_count, 0);
		auto chunk = tlsalloc.allocate();
		ASSERT_EQ(tlsalloc.allocator.storage_active_count, 1);
		ASSERT_EQ(tlsalloc.allocator.total_allocs, 1);
		ASSERT_EQ(tlsalloc.allocator.total_deallocs, 0);
		tlsalloc.deallocate(chunk);
		ASSERT_EQ(tlsalloc.allocator.total_deallocs, 1);
	});

	thread.join();

	tlsalloc.deallocate(chunk);
	ASSERT_EQ(tlsalloc.allocator.total_deallocs, tls_total_dealloc + 1);
}