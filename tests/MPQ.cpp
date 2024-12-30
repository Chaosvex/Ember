/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <mpq/MPQ.h>
#include <mpq/DynamicMemorySink.h>
#include <shared/utility/FileMD5.h>
#include <botan/bigint.h>
#include <gtest/gtest.h>

using namespace ember;

TEST(MPQ, Locate_FileNotFound) {
	const auto result = mpq::locate_archive("test_data/mpqs/no_such_file");
	ASSERT_EQ(result.error(), mpq::ErrorCode::FILE_NOT_FOUND);
}

TEST(MPQ, Locate_ArchiveNotFound) {
	const auto result = mpq::locate_archive("test_data/mpqs/not_an_mpq");
	ASSERT_EQ(result.error(), mpq::ErrorCode::NO_ARCHIVE_FOUND);
}

TEST(MPQ, Locate_ArchiveFound) {
	const auto result = mpq::locate_archive("test_data/mpqs/v0_0.mpq");
	ASSERT_EQ(*result, 0); // offset
}

TEST(MPQ, Locate_ArchiveFoundOffset) {
	const auto result = mpq::locate_archive("test_data/mpqs/v0_1.mpq");
	ASSERT_EQ(*result, 0x200); // offset
}

TEST(MPQ, Locate_ArchiveNotFoundOffset) {
	const auto result = mpq::locate_archive("test_data/mpqs/v0_2.mpq");
	ASSERT_TRUE(!result);
	ASSERT_EQ(result.error(), mpq::ErrorCode::NO_ARCHIVE_FOUND);
}

TEST(MPQ, Locate_BadFile_PartialMagic) {
	const auto result = mpq::locate_archive("test_data/mpqs/v0_3.mpq");
	ASSERT_TRUE(!result);
	ASSERT_EQ(result.error(), mpq::ErrorCode::NO_ARCHIVE_FOUND);
}

TEST(MPQ, Locate_BadHeader_Size) {
	const auto result = mpq::locate_archive("test_data/mpqs/v0_5.mpq");
	ASSERT_TRUE(result);
	ASSERT_THROW(mpq::open_archive("test_data/mpqs/v0_5.mpq", *result), mpq::exception);
}

TEST(MPQ, Locate_BadHeader_Version) {
	const auto result = mpq::locate_archive("test_data/mpqs/v0_6.mpq");
	ASSERT_TRUE(result);
	ASSERT_THROW(mpq::open_archive("test_data/mpqs/v0_6.mpq", *result), mpq::exception);
}

TEST(MPQ, Locate_BadHeader_Truncated) {
	const auto result = mpq::locate_archive("test_data/mpqs/v0_7.mpq");
	ASSERT_TRUE(result);
	ASSERT_THROW(mpq::open_archive("test_data/mpqs/v0_7.mpq", *result), mpq::exception);
}

TEST(MPQ, Open_HeaderTruncated) {
	const auto result = mpq::locate_archive("test_data/mpqs/v0_8.mpq");
	ASSERT_TRUE(result);
	ASSERT_THROW(mpq::open_archive("test_data/mpqs/v0_8.mpq", *result), mpq::exception);
}

TEST(MPQ, Open_BadArchiveSize) {
	const auto result = mpq::locate_archive("test_data/mpqs/v0_9.mpq");
	ASSERT_TRUE(result);
	ASSERT_THROW(mpq::open_archive("test_data/mpqs/v0_9.mpq", *result), mpq::exception);
}

TEST(MPQ, Open_Success) {
	const auto result = mpq::locate_archive("test_data/mpqs/v0_10.mpq");
	ASSERT_TRUE(result);
	ASSERT_NO_THROW(mpq::open_archive("test_data/mpqs/v0_10.mpq", *result));
}

TEST(MPQ, Open_BadHashTableOffset) {
	const auto result = mpq::locate_archive("test_data/mpqs/v0_11.mpq");
	ASSERT_TRUE(result);
	ASSERT_THROW(mpq::open_archive("test_data/mpqs/v0_11.mpq", *result), mpq::exception);
}

TEST(MPQ, Open_BadBlockTableOffset) {
	const auto result = mpq::locate_archive("test_data/mpqs/v0_12.mpq");
	ASSERT_TRUE(result);
	ASSERT_THROW(mpq::open_archive("test_data/mpqs/v0_12.mpq", *result), mpq::exception);
}

TEST(MPQ, Open_BadHashTableSize) {
	const auto result = mpq::locate_archive("test_data/mpqs/v0_13.mpq");
	ASSERT_TRUE(result);
	ASSERT_THROW(mpq::open_archive("test_data/mpqs/v0_13.mpq", *result), mpq::exception);
}

TEST(MPQ, Open_BadBlockTableSize) {
	const auto result = mpq::locate_archive("test_data/mpqs/v0_14.mpq");
	ASSERT_TRUE(result);
	ASSERT_THROW(mpq::open_archive("test_data/mpqs/v0_14.mpq", *result), mpq::exception);
}

TEST(MPQ, Open_BadExtendedBlockTableOffset) {
	const auto result = mpq::locate_archive("test_data/mpqs/v1_15.mpq");
	ASSERT_TRUE(result);
	ASSERT_THROW(mpq::open_archive("test_data/mpqs/v1_15.mpq", *result), mpq::exception);
}

// ADPCM compression, encrypted
TEST(MPQ, Extract_WAV) {
	auto archive = mpq::open_archive("test_data/mpqs/v1_16.mpq", 0);
	ASSERT_TRUE(archive);
	const auto index = archive->file_lookup("owl.wav", 0);
	ASSERT_NE(index, mpq::Archive::npos);
	const auto& entry = archive->file_entry(index);
	mpq::DynamicMemorySink sink;
	ASSERT_NO_THROW(archive->extract_file("owl.wav", sink));
	const auto md5_buf = util::generate_md5(sink.data());
	const auto md5 = Botan::BigInt::decode(md5_buf.data(), md5_buf.size());
	Botan::BigInt expected("0x3a66b4b718686cb4d5aa143895290d84");
	ASSERT_EQ(md5, expected);
}

// No compression
TEST(MPQ, Extract_MP3) {
	auto archive = mpq::open_archive("test_data/mpqs/v1_16.mpq", 0);
	ASSERT_TRUE(archive);
	const auto index = archive->file_lookup("owl.mp3", 0);
	ASSERT_NE(index, mpq::Archive::npos);
	const auto& entry = archive->file_entry(index);
	mpq::DynamicMemorySink sink;
	ASSERT_NO_THROW(archive->extract_file("owl.mp3", sink));
	const auto md5_buf = util::generate_md5(sink.data());
	const auto md5 = Botan::BigInt::decode(md5_buf.data(), md5_buf.size());
	Botan::BigInt expected("0x2f7b030648e1f7ec77109c659bbea3ce");
	ASSERT_EQ(md5, expected);
}

// PKWare compression
TEST(MPQ, Extract_Binary) {
	auto archive = mpq::open_archive("test_data/mpqs/v1_16.mpq", 0);
	ASSERT_TRUE(archive);
	const auto index = archive->file_lookup("elevated_1920_1080.ex_", 0);
	ASSERT_NE(index, mpq::Archive::npos);
	const auto& entry = archive->file_entry(index);
	mpq::DynamicMemorySink sink;
	ASSERT_NO_THROW(archive->extract_file("elevated_1920_1080.ex_", sink));
	const auto md5_buf = util::generate_md5(sink.data());
	const auto md5 = Botan::BigInt::decode(md5_buf.data(), md5_buf.size());
	Botan::BigInt expected("0xf3b82b404a36a9714bb266007cacd489");
	ASSERT_EQ(md5, expected);
}

// Imploded
TEST(MPQ, Extract_JPG) {
	auto archive = mpq::open_archive("test_data/mpqs/v1_16.mpq", 0);
	ASSERT_TRUE(archive);
	const auto& index = archive->file_lookup("ember.jpg", 0);
	ASSERT_NE(index, mpq::Archive::npos);
	const auto& entry = archive->file_entry(index);
	mpq::DynamicMemorySink sink;
	ASSERT_NO_THROW(archive->extract_file("ember.jpg", sink));
	const auto md5_buf = util::generate_md5(sink.data());
	const auto md5 = Botan::BigInt::decode(md5_buf.data(), md5_buf.size());
	Botan::BigInt expected("0x8a623acdf09f9388719010d76a5c7a52");
	ASSERT_EQ(md5, expected);
}

// Zlib compression, encrypted, single unit, fix key
TEST(MPQ, Extract_PNG) {
	auto archive = mpq::open_archive("test_data/mpqs/v1_16.mpq", 0);
	ASSERT_TRUE(archive);
	const auto index = archive->file_lookup("ember.png", 0);
	ASSERT_NE(index, mpq::Archive::npos);
	const auto& entry = archive->file_entry(index);
	mpq::DynamicMemorySink sink;
	ASSERT_NO_THROW(archive->extract_file("ember.png", sink));
	const auto md5_buf = util::generate_md5(sink.data());
	const auto md5 = Botan::BigInt::decode(md5_buf.data(), md5_buf.size());
	Botan::BigInt expected("0xb29a1ad8ebeef28de68fa9bb0325b893");
	ASSERT_EQ(md5, expected);
}

TEST(MPQ, Extract_Listfile) {
	auto archive = mpq::open_archive("test_data/mpqs/v1_16.mpq", 0);
	ASSERT_TRUE(archive);
	const auto index = archive->file_lookup("(listfile)", 0);
	ASSERT_NE(index, mpq::Archive::npos);
	const auto& entry = archive->file_entry(index);
	mpq::DynamicMemorySink sink;
	ASSERT_NO_THROW(archive->extract_file("(listfile)", sink));
	const auto md5_buf = util::generate_md5(sink.data());
	const auto md5 = Botan::BigInt::decode(md5_buf.data(), md5_buf.size());
	Botan::BigInt expected("0xc4185e8d87a01ac3057a6498deccab1e");
	ASSERT_EQ(md5, expected);
}

TEST(MPQ, Extract_Attributes) {
	auto archive = mpq::open_archive("test_data/mpqs/v1_16.mpq", 0);
	ASSERT_TRUE(archive);
	const auto index = archive->file_lookup("(attributes)", 0);
	ASSERT_NE(index, mpq::Archive::npos);
	const auto& entry = archive->file_entry(index);
	mpq::DynamicMemorySink sink;
	ASSERT_NO_THROW(archive->extract_file("(attributes)", sink));
	const auto md5_buf = util::generate_md5(sink.data());
	const auto md5 = Botan::BigInt::decode(md5_buf.data(), md5_buf.size());
	Botan::BigInt expected("0x44fd6bad334ea24d8901e80ab20c07ba");
	ASSERT_EQ(md5, expected);
}

TEST(MPQ, Extract_Text) {
	auto archive = mpq::open_archive("test_data/mpqs/v1_16.mpq", 0);
	ASSERT_TRUE(archive);
	const auto index = archive->file_lookup("compressed.txt", 0);
	ASSERT_NE(index, mpq::Archive::npos);
	const auto& entry = archive->file_entry(index);
	mpq::DynamicMemorySink sink;
	ASSERT_NO_THROW(archive->extract_file("compressed.txt", sink));
	const auto md5_buf = util::generate_md5(sink.data());
	const auto md5 = Botan::BigInt::decode(md5_buf.data(), md5_buf.size());
	Botan::BigInt expected("0xf13fad545731a0b71b2cc17e1acd48f4");
	ASSERT_EQ(md5, expected);
}

TEST(MPQ, Locate_BadAlignment) {
	alignas(mpq::v0::Header) std::byte data[mpq::HEADER_SIZE_V0]{};
	std::span span(data + 1, sizeof(data) - 1); // force misaligned buffer
	auto result = mpq::locate_archive(span);
	ASSERT_EQ(result.error(), mpq::ErrorCode::BAD_ALIGNMENT);
}