/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <login/IntegrityData.h>
#include <shared/utility/FileMD5.h>
#include <gtest/gtest.h>
#include <array>
#include <cstdint>

using namespace ember;

TEST(IntegrityData, LoadData_MD5) {
	const GameVersion version {
		.major = 1,
		.minor = 12,
		.build = 5875,
	};
	
	IntegrityData data;
	data.add_version(version, "test_data/");
	const auto res = data.lookup(version, grunt::Platform::x86, grunt::System::Win);
	ASSERT_TRUE(res);
	ASSERT_EQ(res->size_bytes(), 320);

	const auto md5 = util::generate_md5(*res);
	const std::array<std::uint8_t, 16> expected_md5 {
		0xd6, 0x4a, 0xd1, 0xb3, 0x86, 0x02, 0x08, 0x76, 
		0x2d, 0xfd, 0xd1, 0x0a, 0x9b, 0x85, 0x75, 0xbd
	};

	ASSERT_EQ(md5, expected_md5);
}

TEST(IntegrityData, LoadData_NotFound_BadDir) {
	const GameVersion version {
		.major = 1,
		.minor = 12,
		.build = 5875,
	};

	IntegrityData data;
	ASSERT_ANY_THROW(data.add_version(version, "fake_dir/"));
}

TEST(IntegrityData, LoadData_NotFound_BadVers) {
	const GameVersion version {
		.major = 2,
		.minor = 12,
		.build = 1000,
	};

	IntegrityData data;
	ASSERT_ANY_THROW(data.add_version(version, "test_data/"));
}