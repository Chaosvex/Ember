/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Utility.h"
#include <shared/CompilerWarn.h>

#ifdef _WIN32
    #include <Windows.h>
#elif defined __linux__ || defined __unix__ || defined TARGET_OS_MAC
	#include <sys/resource.h>
	#include <iostream>
#endif

// signal stuff
#if defined _WIN32 || defined TARGET_OS_MAC
	#include <signal.h>
#elif defined __linux__ || defined __unix__
	#include <format>
	#include <string.h>
#endif

// page locking stuff
#if defined _WIN32
	#include <memoryapi.h>
#elif defined __linux__ || defined __unix__  || defined TARGET_OS_MAC
	#include <sys/mman.h>
#endif

namespace ember::util {

std::size_t max_consecutive(std::string_view name, const bool case_insensitive,
                            const std::locale& locale) {
	std::size_t current_run = 0;
	std::size_t longest_run = 0;
	char last = 0;

	for(auto c : name) {
		if(case_insensitive) {
			c = std::tolower(c, locale);
		}

		if(c == last) {
			++current_run;
		} else {
			current_run = 1;
		}

		if(current_run > longest_run) {
			longest_run = current_run;
		}

		last = c;
	}

	return longest_run;
}

void set_window_title(cstring_view title) {
#ifdef _WIN32
    SetConsoleTitle(title.c_str());
#elif defined __linux__ || defined __unix__ // todo, test
  	std::cout << "\033]0;" << title << "}\007";
#endif
}

int max_sockets() {
#if defined __linux__ || defined __unix__ || defined TARGET_OS_MAC
	rlimit limit{};
	const int res = getrlimit(RLIMIT_NOFILE, &limit);
	
	if(res == 0) {
		return -1;
	}

	return limit.rlim_cur;
#endif
	return 0;
}

std::string max_sockets_desc() {
	int max = max_sockets();
	std::string value;

	if(max == -1) {
		value = "unable to retrieve value";
	} else if(max == 0) {
		value = "no known limits";
	} else {
		value = std::to_string(max);
	}

	return value;
}

#define STRINGIZE_CASE(x) \
case x: \
	return #x;

std::string sig_str(const int signal) {
#if defined _WIN32 || defined TARGET_OS_MAC
	switch(signal) {
		STRINGIZE_CASE(SIGINT)
		STRINGIZE_CASE(SIGILL)
		STRINGIZE_CASE(SIGFPE)
		STRINGIZE_CASE(SIGSEGV)
		STRINGIZE_CASE(SIGTERM)
#endif
#ifdef  _WIN32
		STRINGIZE_CASE(SIGBREAK)
		STRINGIZE_CASE(SIGABRT)
#elif defined TARGET_OS_MAC
		STRINGIZE_CASE(SIGHUP)
		STRINGIZE_CASE(SIGQUIT)
		STRINGIZE_CASE(SIGTRAP)
		STRINGIZE_CASE(SIGABRT)
		STRINGIZE_CASE(SIGEMT)
		STRINGIZE_CASE(SIGKILL)
		STRINGIZE_CASE(SIGBUS)
		STRINGIZE_CASE(SIGSYS)
		STRINGIZE_CASE(SIGPIPE)
		STRINGIZE_CASE(SIGALRM)
		STRINGIZE_CASE(SIGURG)
		STRINGIZE_CASE(SIGSTOP)
		STRINGIZE_CASE(SIGTSTP)
		STRINGIZE_CASE(SIGCONT)
		STRINGIZE_CASE(SIGCHLD)
		STRINGIZE_CASE(SIGTTIN)
		STRINGIZE_CASE(SIGTTOU)
		STRINGIZE_CASE(SIGIO)
		STRINGIZE_CASE(SIGXCPU)
		STRINGIZE_CASE(SIGXFSZ)
		STRINGIZE_CASE(SIGVTALRM)
		STRINGIZE_CASE(SIGPROF)
		STRINGIZE_CASE(SIGWINCH)
		STRINGIZE_CASE(SIGINFO)
		STRINGIZE_CASE(SIGUSR1)
		STRINGIZE_CASE(SIGUSR2)
#endif
#if defined _WIN32 || defined TARGET_OS_MAC
		default:
			return "<unknown>";
	}
#elif defined __linux__ || defined __unix__
	auto res = sigabbrev_np(signal);

	if(res) {
		return std::format("SIG{}", res);
	} else {
		return "<unknown>";
	}
#else
	#pragma message WARN("Implement sig_str for this platform, thanks");
#endif
}

bool page_lock(void* address, std::size_t length) {
#if defined _WIN32
	return VirtualLock(address, length) != 0;
#elif defined __linux__ || defined __unix__|| defined TARGET_OS_MAC
	return mlock(address, length) == 0;
#else
	#pragma message WARN("Implement page_lock for this platform, thanks");
	return false;
#endif
}

bool page_unlock(void* address, std::size_t length) {
#if defined _WIN32
	return VirtualUnlock(address, length) != 0;
#elif defined __linux__ || defined __unix__|| defined TARGET_OS_MAC
	return munlock(address, length) == 0;
#else
	#pragma message WARN("Implement page_unlock for this platform, thanks");
	return false;
#endif
}

} // util, ember
