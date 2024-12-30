/*
 * Copyright (c) 2022 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <shared/utility/UTF8.h>
#include <shared/utility/Utility.h>
#include <gtest/gtest.h>
#include <string>

using namespace ember;

/*
 * Most UTF8 test cases are adapted from:
 * https://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt
 * 
 * Almost all of these are basic tests for whichever UTF8
 * library is being used by the utilities rather than Ember's
 * own code
 */

TEST(TextUtilities, StringLength_ASCII) {
	std::string str = "Pretty boring test text";
	auto len = util::utf8::length(str);
	EXPECT_EQ(len, 23);

	str = "";
	len = util::utf8::length(str);
	EXPECT_EQ(len, 0);
}

TEST(TextUtilities, StringLength_UTF8) {
	std::string str = "日本語テキスト";
	auto len = util::utf8::length(str);
	EXPECT_EQ(len, 7);

	str = "ŏϭեۛåȆɷςƐҕђǹǹǍ";
	len = util::utf8::length(str);
	EXPECT_EQ(len, 14);

	str = "翻译不好的文本";
	len = util::utf8::length(str);
	EXPECT_EQ(len, 7);
	
	str = "";
	len = util::utf8::length(str);
	EXPECT_EQ(len, 0);
}

TEST(TextUtilities, DISABLED_NameFormat_ASCII) {
	// Lower
	std::string str = "bankalt";
	auto formatted = util::utf8::name_format(str, std::locale());
	EXPECT_EQ("Bankalt", formatted);

	// Upper
	str = "CHAOSVEX";
	formatted = util::utf8::name_format(str, std::locale());
	EXPECT_EQ("Chaosvex", formatted);

	// Mixed
	str = "cHaoSvEx";
	formatted = util::utf8::name_format(str, std::locale());
	EXPECT_EQ("Chaosvex", formatted);
}

TEST(TextUtilities, IsValid_UTF8_MixedSequences) {
	std::string text = "CHAOSVEX";
	bool res = util::utf8::is_valid(text);
	EXPECT_EQ(res, true);

	text = "日本語テキスト";
	res = util::utf8::is_valid(text);
	EXPECT_EQ(res, true);

	const char* bad_text = "\xf0\x28\x8c\x28";
	res = util::utf8::is_valid(bad_text, 4);
	EXPECT_EQ(res, false);
}

TEST(TextUtilities, IsValid_UTF8_OverlongSequences) {
	{
		const char* bad_text = "\xc0\xaf";
		auto res = util::utf8::is_valid(bad_text, 2);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xe0\x80\xaf";
		auto res = util::utf8::is_valid(bad_text, 3);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xf0\x80\x80\xaf";
		auto res = util::utf8::is_valid(bad_text, 4);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xf8\x80\x80\x80\xaf";
		auto res = util::utf8::is_valid(bad_text, 5);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xfc\x80\x80\x80\x80\xaf";
		auto res = util::utf8::is_valid(bad_text, 6);
		EXPECT_EQ(res, false);
	}
}

TEST(TextUtilities, IsValid_UTF8_UnexpectedContinuation) {
	{
		const char* bad_text = "\x80";
		auto res = util::utf8::is_valid(bad_text, 1);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xbf";
		auto res = util::utf8::is_valid(bad_text, 1);
		EXPECT_EQ(res, false);
	}
}

TEST(TextUtilities, IsValid_UTF8_OverlongNul) {
	{
		const char* bad_text = "\xc0\x80";
		auto res = util::utf8::is_valid(bad_text, 2);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xe0\x80\x80";
		auto res = util::utf8::is_valid(bad_text, 3);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xf0\x80\x80\x80";
		auto res = util::utf8::is_valid(bad_text, 4);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xf8\x80\x80\x80\x80";
		auto res = util::utf8::is_valid(bad_text, 5);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xfc\x80\x80\x80\x80\x80";
		auto res = util::utf8::is_valid(bad_text, 6);
		EXPECT_EQ(res, false);
	}
}

TEST(TextUtilities, IsValid_UTF8_SingleUTF16Surrogates) {
	{
		const char* bad_text = "\xed\xa0\x80";
		auto res = util::utf8::is_valid(bad_text, 3);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xed\xad\xbf";
		auto res = util::utf8::is_valid(bad_text, 3);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xed\xae\x80";
		auto res = util::utf8::is_valid(bad_text, 3);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xed\xaf\xbf";
		auto res = util::utf8::is_valid(bad_text, 3);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xed\xb0\x80";
		auto res = util::utf8::is_valid(bad_text, 3);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xed\xbe\x80";
		auto res = util::utf8::is_valid(bad_text, 3);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xed\xbf\xbf";
		auto res = util::utf8::is_valid(bad_text, 3);
		EXPECT_EQ(res, false);
	}
}

TEST(TextUtilities, IsValid_UTF8_PairedUTF16Surrogates) {
	{
		const char* bad_text = "\xed\xa0\x80\xed\xb0\x80";
		auto res = util::utf8::is_valid(bad_text, 6);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xed\xa0\x80\xed\xbf\xbf";
		auto res = util::utf8::is_valid(bad_text, 6);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xed\xad\xbf\xed\xb0\x80";
		auto res = util::utf8::is_valid(bad_text, 6);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xed\xad\xbf\xed\xbf\xbf";
		auto res = util::utf8::is_valid(bad_text, 6);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xed\xae\x80\xed\xb0\x80";
		auto res = util::utf8::is_valid(bad_text, 6);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xed\xae\x80\xed\xbf\xbf";
		auto res = util::utf8::is_valid(bad_text, 6);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xed\xaf\xbf\xed\xb0\x80";
		auto res = util::utf8::is_valid(bad_text, 6);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xed\xaf\xbf\xed\xbf\xbf";
		auto res = util::utf8::is_valid(bad_text, 6);
		EXPECT_EQ(res, false);
	}
}

TEST(TextUtilities, IsValid_UTF8_MaxOverlongSequences) {
	{
		const char* bad_text = "\xc1\xbf";
		auto res = util::utf8::is_valid(bad_text, 2);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xe0\x9f\xbf";
		auto res = util::utf8::is_valid(bad_text, 3);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xf0\x8f\xbf\xbf";
		auto res = util::utf8::is_valid(bad_text, 4);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xf8\x87\xbf\xbf\xbf";
		auto res = util::utf8::is_valid(bad_text, 5);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xfc\x83\xbf\xbf\xbf\xbf";
		auto res = util::utf8::is_valid(bad_text, 6);
		EXPECT_EQ(res, false);
	}
}

TEST(TextUtilities, IsValid_UTF8_ImpossibleBytes) {
	{
		const char* bad_text = "\xfe\xfe\xff\xff";
		auto res = util::utf8::is_valid(bad_text, 4);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xfe";
		auto res = util::utf8::is_valid(bad_text, 1);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xff";
		auto res = util::utf8::is_valid(bad_text, 1);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "\xff\xfe";
		auto res = util::utf8::is_valid(bad_text, 2);
		EXPECT_EQ(res, false);
	}

	{
		const char* bad_text = "�����";
		auto res = util::utf8::is_valid(bad_text, 5);
		EXPECT_EQ(res, false);
	}
}

TEST(TextUtilities, IsValid_UTF8_Noncharacters) {
	{
		const char* bad_text = "﷐﷑﷒﷓﷔﷕﷖﷗﷘﷙﷚﷛﷜﷝﷞﷟﷠﷡﷢﷣﷤﷥﷦﷧﷨﷩﷪﷫﷬﷭﷮﷯";
		auto res = util::utf8::is_valid(bad_text, 32);
		EXPECT_EQ(res, false);
	}
}

TEST(TextUtilities, ConsecutiveCheck_ASCII_Case_Insensitive) {
	std::string text = "";
	auto res = util::max_consecutive(text);
	EXPECT_EQ(res, 0);

	text = "f";
	res = util::max_consecutive(text);
	EXPECT_EQ(res, 1);

	text = "ff";
	res = util::max_consecutive(text);
	EXPECT_EQ(res, 2);

	text = "fff";
	res = util::max_consecutive(text);
	EXPECT_EQ(res, 3);

	text = "foo";
	res = util::max_consecutive(text);
	EXPECT_EQ(res, 2);

	text = "foofooof";
	res = util::max_consecutive(text);
	EXPECT_EQ(res, 3);

	text = "FOOBAAAR";
	res = util::max_consecutive(text);
	EXPECT_EQ(res, 3);

	text = "FOOBAaAR";
	res = util::max_consecutive(text);
	EXPECT_EQ(res, 2);
}

TEST(TextUtilities, ConsecutiveCheck_ASCII_CaseSensitive) {
	std::string text = "";
	auto res = util::max_consecutive(text, true);
	EXPECT_EQ(res, 0);

	text = "Fff";
	res = util::max_consecutive(text, true);
	EXPECT_EQ(res, 3);

	text = "FffOooo";
	res = util::max_consecutive(text, true);
	EXPECT_EQ(res, 4);
}

TEST(TextUtilities, DISABLED_ConsecutiveCheck_UTF8_CaseInsensitive) {
	std::string text = "";
	auto res = util::utf8::max_consecutive(text, true);
	EXPECT_EQ(res, 0);

	text = "Fff";
	res = util::utf8::max_consecutive(text, true);
	EXPECT_EQ(res, 3);

	text = "FffOooo";
	res = util::utf8::max_consecutive(text, true);
	EXPECT_EQ(res, 4);
}

TEST(TextUtilities, ConsecutiveCheck_UTF8_CaseSensitive) {
	std::string text = "";
	auto res = util::utf8::max_consecutive(text);
	EXPECT_EQ(res, 0);

	text = "テキスト";
	res = util::utf8::max_consecutive(text);
	EXPECT_EQ(res, 1);

	text = "テテキスト";
	res = util::utf8::max_consecutive(text);
	EXPECT_EQ(res, 2);

	text = "テテキススス";
	res = util::utf8::max_consecutive(text);
	EXPECT_EQ(res, 3);

	text = "テテキスススト";
	res = util::utf8::max_consecutive(text);
	EXPECT_EQ(res, 3);
}