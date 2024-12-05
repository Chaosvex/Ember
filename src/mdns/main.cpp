/*
 * Copyright (c) 2021 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Runner.h"
#include <shared/Banner.h>
#include <shared/Version.h>
#include <shared/threading/Utility.h>
#include <shared/util/Utility.h>
#include <iostream>

using namespace ember;

/*
 * We want to do the minimum amount of work required to get 
 * logging facilities and crash handlers up and running in main.
 *
 * Exceptions that aren't derived from std::exception are
 * left to the crash handler since we can't get useful information
 * from them.
 */
int main(int argc, const char* argv[]) try {
	thread::set_name("Main");
	print_banner(dns::APP_NAME);
	util::set_window_title(dns::APP_NAME);

	std::span<const char*> args(argv, argc);
	return dns::run(args);
} catch(std::exception& e) {
	std::cerr << e.what();
	return EXIT_FAILURE;
}
