/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <spark/buffers/detail/SharedDefs.h>
#include <gtest/gtest.h>
#include <array>
#include <cstdint>

using namespace ember;

TEST(BufferUtility, SrcDestOverlap_Start) {
	std::array<std::uint8_t, 10> buffer{};
	const auto begin = buffer.data();
	const auto end = buffer.data() + buffer.size();
	ASSERT_TRUE(spark::io::region_overlap(buffer.data(), buffer.size(), buffer.data(), buffer.size()));
}

TEST(BufferUtility, SrcDestOverlap_End) {
	std::array<std::uint8_t, 10> buffer{};
	const auto begin = buffer.data();
	const auto end = buffer.data() + buffer.size();
	ASSERT_TRUE(spark::io::region_overlap(buffer.data(), buffer.size(), end - 1, 1));
}

TEST(BufferUtility, SrcDestOverlap_BeyondEnd) {
	std::array<std::uint8_t, 10> buffer{};
	const auto begin = buffer.data();
	const auto end = buffer.data() + buffer.size();
	ASSERT_FALSE(spark::io::region_overlap(buffer.data(), buffer.size(), end, 1));
}

TEST(BufferUtility, SrcDestOverlap_Between) {
	std::array<std::uint8_t, 10> buffer{};
	ASSERT_TRUE(spark::io::region_overlap(buffer.data(), buffer.size(), buffer.data() + (buffer.size() - 5), 1));
}

TEST(BufferUtility, SrcDestOverlap_NoOverlap) {
	std::array<std::uint8_t, 10> buffer{}, buffer2{};
	ASSERT_FALSE(spark::io::region_overlap(buffer.data(), buffer.size(), buffer2.data(), buffer2.size()));
}