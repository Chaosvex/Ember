/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <login/Patcher.h>
#include <shared/database/daos/shared_base/PatchBase.h>
#include <gtest/gtest.h>
#include <numeric>
#include <span>
#include <vector>
#include <cstdint>

using namespace ember;

class MockPatchDAO final : public dal::PatchDAO {
public:
	mutable int update_count = 0;
	mutable bool rollup;

	explicit MockPatchDAO(bool include_rollup) : rollup(include_rollup) {}

	std::vector<PatchMeta> fetch_patches() const override {
		std::vector<PatchMeta> patches {
			{0, 1, 2, 0, 0, 0, "x86", "enGB", "Win", false, false, "", "1_to_2.patch", {}, 0},
			{1, 2, 3, 0, 0, 0, "x86", "enGB", "Win", false, false, "", "2_to_3.patch", {}, 0},
			{2, 3, 4, 0, 0, 0, "x86", "enGB", "Win", false, false, "", "3_to_4.patch", {}, 0}
		};

		if(rollup) {
			patches.emplace_back(
				PatchMeta{ 3, 1, 4, 0, 0, 0, "x86", "enGB", "Win", false, true,  "", "1_to_4.patch", {}, 0 }
			);
		}

		return patches;
	};

	void update(const PatchMeta& meta) const override {
		++update_count;
	}
};

std::vector<GameVersion> versions {
	{ 1, 2, 1, 5875 },
	{ 1, 2, 1, 6005 }
};

TEST(PatcherTest, VersionChecks) {
	GameVersion version_ok { 1, 2, 1, 5875 };
	GameVersion version_too_new { 2, 0, 1, 6180 };
	GameVersion version_too_old { 1, 1, 0, 4044 };
	Patcher patcher(versions, {});
	
	ASSERT_EQ(Patcher::PatchLevel::OK, patcher.check_version(version_ok))
		<< "Build should have been accepted";

	ASSERT_EQ(Patcher::PatchLevel::TOO_NEW, patcher.check_version(version_too_new))
		<< "Build should have been rejected as too new";

	ASSERT_EQ(Patcher::PatchLevel::TOO_OLD, patcher.check_version(version_too_old))
		<< "Build should have been rejected as too old";
}

TEST(PatcherTest, LoadMD5) {
	MockPatchDAO dao(true);
	const auto meta = Patcher::load_patches("test_data/patches/", dao);
	ASSERT_EQ(dao.update_count, 4);
	ASSERT_EQ(meta.size(), 4);

	const std::array<std::uint8_t, 16> md5_1_to_2 {
		0x55, 0xa5, 0x40, 0x08, 0xad, 0x1b, 0xa5, 0x89,
		0xaa, 0x21, 0x0d, 0x26, 0x29, 0xc1, 0xdf, 0x41
	};

	const std::array<std::uint8_t, 16> md5_2_to_3 {
		0x9e, 0x68, 0x8c, 0x58, 0xa5, 0x48, 0x7b, 0x8e,
		0xaf, 0x69, 0xc9, 0xe1, 0x00, 0x5a, 0xd0, 0xbf
	};

	const std::array<std::uint8_t, 16> md5_3_to_4 {
		0x86, 0x66, 0x68, 0x35, 0x06, 0xaa, 0xcd, 0x90,
		0x0b, 0xbd, 0x5a, 0x74, 0xac, 0x4e, 0xdf, 0x68
	};

	const std::array<std::uint8_t, 16> md5_1_to_4 {
		0xec, 0x7f, 0x7e, 0x7b, 0xb4, 0x37, 0x42, 0xce,
		0x86, 0x81, 0x45, 0xf7, 0x1d, 0x37, 0xb5, 0x3c
	};

	std::array<bool, 4> matches{};

	// std::byte was a mistake
	for(const auto& m : meta) {
		if(m.file_meta.name == "1_to_2.patch") {
			ASSERT_TRUE(std::ranges::equal(m.file_meta.md5, md5_1_to_2));
			ASSERT_EQ(m.file_meta.size, 1);
			matches[0] = true;
		} else if(m.file_meta.name == "2_to_3.patch") {
			ASSERT_TRUE(std::ranges::equal(m.file_meta.md5, md5_2_to_3));
			ASSERT_EQ(m.file_meta.size, 1);
			matches[1] = true;
		} else if(m.file_meta.name == "3_to_4.patch") {
			ASSERT_TRUE(std::ranges::equal(m.file_meta.md5, md5_3_to_4));
			ASSERT_EQ(m.file_meta.size, 1);
			matches[2] = true;
		} else if(m.file_meta.name == "1_to_4.patch") {
			ASSERT_TRUE(std::ranges::equal(m.file_meta.md5, md5_1_to_4));
			ASSERT_EQ(m.file_meta.size, 1);
			matches[3] = true;
		} else {
			ASSERT_TRUE(false) << "Shouldn't have made it here";
		}
	}

	const auto val = std::accumulate(matches.begin(), matches.end(), 0);
	ASSERT_EQ(meta.size(), val);
}

TEST(PatcherTest, IncrementalPatch) {
	const std::vector<GameVersion> supported {
		{ 0, 0, 0, 4 }
	};

	// excluding the rollup patch to test patch pathing as
	// the algorithm will choose the rollup as the optimal path
	// if it finds one (1 patch vs 3)
	MockPatchDAO dao(false);
	const auto meta = Patcher::load_patches("test_data/patches/", dao);
	Patcher patcher(supported, meta);

	auto patch = patcher.find_patch(
		GameVersion{ 0, 0, 0, 1 }, grunt::Locale::enGB,
		grunt::Platform::x86, grunt::System::Win
	);

	ASSERT_TRUE(patch);
	ASSERT_EQ(patch->file_meta.name, "1_to_2.patch");

	patch = patcher.find_patch(
		GameVersion{ 0, 0, 0, 2 }, grunt::Locale::enGB,
		grunt::Platform::x86, grunt::System::Win
	);

	ASSERT_TRUE(patch);
	ASSERT_EQ(patch->file_meta.name, "2_to_3.patch");

	patch = patcher.find_patch(
		GameVersion{ 0, 0, 0, 3 }, grunt::Locale::enGB,
		grunt::Platform::x86, grunt::System::Win
	);

	ASSERT_TRUE(patch);
	ASSERT_EQ(patch->file_meta.name, "3_to_4.patch");
}

TEST(PatcherTest, RollupPatch) {
	std::vector<GameVersion> supported {
		{ 0, 0, 0, 4 }
	};

	MockPatchDAO dao(true);
	const auto meta = Patcher::load_patches("test_data/patches/", dao);
	Patcher patcher(supported, meta);

	const auto patch = patcher.find_patch(
		GameVersion{ 0, 0, 0, 4 }, grunt::Locale::enGB,
		grunt::Platform::x86, grunt::System::Win
	);

	ASSERT_TRUE(patch);
	ASSERT_EQ(patch->file_meta.name, "1_to_4.patch");
}