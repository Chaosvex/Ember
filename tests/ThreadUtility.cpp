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
	thread::set_name(set_name);
	const std::wstring wname(set_name, set_name + strlen(set_name));
	const auto name = thread::get_name();

	// todo: bit hacky
	if(name == L"unsupported") {
		ASSERT_TRUE(true);
		return;
	}

	ASSERT_EQ(name, wname);
}

// only running on Linux/Unix distros for now
TEST(ThreadUtility, GetSetName) {
	std::binary_semaphore sem(0);
	const char* set_name = "Test Name";
	const std::wstring wname(set_name, set_name + strlen(set_name));

	std::thread thread([&]() {
		sem.acquire();
	});

	thread::set_name(thread, set_name);
	const auto name = thread::get_name(thread);

	// todo: bit hacky
	if(name == L"unsupported") {
		ASSERT_TRUE(true);
		return;
	}

	ASSERT_EQ(name, wname);
	sem.release();
	thread.join();
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
