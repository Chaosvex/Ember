/*
* Copyright (c) 2024 Ember
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <shared/threading/Utility.h>
#include <gtest/gtest.h>
#include <semaphore>
#include <string>
#include <thread>
#include <cstring>

using namespace ember;

// only running on Linux/Unix distros for now
TEST(ThreadUtility, Self_GetSetName) {
	const char* set_name = "Test Name";
	
	if(thread::set_name(set_name) == thread::Result::unsupported) {
		GTEST_SKIP_("unsupported on platform");
	}

	const std::wstring wname(set_name, set_name + strlen(set_name));
	const auto name = thread::get_name();

	if(!name && name.error() == thread::Result::unsupported) {
		GTEST_SKIP_("unsupported on platform");
	}

	ASSERT_EQ(name, wname);
}

// only running on Linux/Unix distros for now
TEST(ThreadUtility, GetSetName) {
	std::binary_semaphore sem(0);
	const char* set_name = "Test Name";
	const std::wstring wname(set_name, set_name + strlen(set_name));

	std::jthread thread([&]() {
		sem.acquire();
	});

	if(thread::set_name(thread, set_name) == thread::Result::unsupported) {
		sem.release();
		GTEST_SKIP_("unsupported on platform");
	}

	const auto name = thread::get_name(thread);

	if(!name && name.error() == thread::Result::unsupported) {
		sem.release();
		GTEST_SKIP_("unsupported on platform");
	}

	ASSERT_EQ(name, wname);
	sem.release();
}

TEST(ThreadUtility, MaxNameLen) {
	ASSERT_NO_THROW(thread::set_name("Max name length"));
}

TEST(ThreadUtility, NameTooLongBoundary) {
	ASSERT_ANY_THROW(thread::set_name("Name is too long"));
}

TEST(ThreadUtility, NameTooLong) {
	ASSERT_ANY_THROW(thread::set_name("This thread name is far too long to be valid"));
}
