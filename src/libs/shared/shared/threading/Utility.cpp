/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Utility.h"
#include <shared/CompilerWarn.h>
#include <array>
#include <stdexcept>
#include <string>
#include <cstring>

#ifdef _WIN32
#include <Windows.h>
#include <cwchar>
#elif defined TARGET_OS_MAC
#include <pthread.h>
#elif defined __linux__ || defined __unix__
#include <sched.h>
#include <pthread.h>
#endif

constexpr auto BUFFER_LEN   = 32u;
constexpr auto MAX_NAME_LEN = 16u; // includes null term

namespace ember::thread {

#ifdef _WIN32
typedef HRESULT (__stdcall *SetThreadDescription)(HANDLE hThread, PCWSTR lpThreadDescription);
typedef HRESULT (__stdcall *GetThreadDescription)(HANDLE hThread, PWSTR* ppszThreadDescription);
#endif

void set_affinity(auto thread, unsigned int core) {
#ifdef _WIN32
	if(SetThreadAffinityMask(thread, 1u << core) == 0) {
		throw std::runtime_error("Unable to set thread affinity, error code " + std::to_string(GetLastError()));
	}
#elif defined TARGET_OS_MAC
	// todo, no support for pthread_setaffinity_np - implement when I'm in a position to test it
#elif defined __linux__ || defined __unix__
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(core, &mask);
	auto ret = pthread_setaffinity_np(thread, sizeof(mask), &mask);

	if(ret) {
		throw std::runtime_error("Unable to set thread affinity, error code " + std::to_string(ret));
	}
#else
	#pragma message WARN("Setting thread affinity is not implemented for this platform. Implement it, please!");
#endif
}

void set_affinity(std::thread& thread, unsigned int core) {
	set_affinity(thread.native_handle(), core);
}

void set_affinity(std::jthread& thread, unsigned int core) {
	set_affinity(thread.native_handle(), core);
}

void set_name([[maybe_unused]] auto& handle, const char* name) {
	if(strlen(name) >= MAX_NAME_LEN) {
		throw std::runtime_error("set_name: thread name too long");
	}

#if defined _WIN32
	auto lib = LoadLibrary("Kernel32.dll");

	if(!lib) {
		return;
	}

	auto set_thread_desc = reinterpret_cast<SetThreadDescription>(GetProcAddress(lib, "SetThreadDescription"));

	if(!set_thread_desc) {
		return;
	}

	const std::wstring wstr(name, name + strlen(name));
	auto ret = set_thread_desc(handle, wstr.c_str());

	if(FAILED(ret)) {
		throw std::runtime_error("Unable to set thread name, error code" + std::to_string(ret));
	}
#elif defined TARGET_OS_MAC
	auto ret = pthread_setname_np(name);

	if(ret) {
		throw std::runtime_error("Unable to set thread name, error code" + std::to_string(ret));
	}
#elif defined __linux__ || defined __unix__
	auto ret = pthread_setname_np(handle, name);

	if(ret) {
		throw std::runtime_error("Unable to set thread name, error code" + std::to_string(ret));
	}
#endif
}

void set_name(std::jthread& thread, const char* name) {
#ifndef TARGET_OS_MAC
	const auto handle = thread.native_handle();
	set_name(handle, name);
#else
#pragma message WARN("Setting thread names is not implemented for this platform. Implement it, please!");
#endif
}

void set_name(std::thread& thread, const char* name) {
#ifndef TARGET_OS_MAC
	const auto handle = thread.native_handle();
	set_name(handle, name);
#else
#pragma message WARN("Setting thread names is not implemented for this platform. Implement it, please!");
#endif
}

void set_name(const char* name) {
#ifdef _WIN32
	auto handle = GetCurrentThread();
	set_name(handle, name);
#elif defined __linux__ || defined __unix__ || defined TARGET_OS_MAC
	auto handle = pthread_self();
	set_name(handle, name);
#else
#pragma message WARN("Setting thread names is not implemented for this platform. Implement it, please!");
#endif
}

std::wstring get_name(auto& thread) {
#if defined _WIN32
	auto lib = LoadLibrary("Kernel32.dll");

	if(!lib) {
		return L"unsupported";
	}

	auto get_thread_desc = reinterpret_cast<GetThreadDescription>(GetProcAddress(lib, "GetThreadDescription"));

	if(!get_thread_desc) {
		return L"unsupported";
	}

	std::array<wchar_t, BUFFER_LEN> buffer{};
	wchar_t* pbuffer = buffer.data();
	auto res = get_thread_desc(thread, &pbuffer);

	if(FAILED(res)) {
		throw std::runtime_error("Unable to get thread name, error code" + std::to_string(res));
	}

	return std::wstring(buffer.data(), buffer.data() + wcslen(buffer.data()));
#elif defined TARGET_OS_MAC
	std::array<char, BUFFER_LEN> buffer{};
	auto res = pthread_getname_np(buffer.data(), buffer.size()); // todo, taking a guess here, can't test

	if(res) {
		throw std::runtime_error("Unable to get thread name, error code" + std::to_string(res));
	}

	return std::wstring(buffer, buffer + strlen(buffer));
#elif defined __linux__ || defined __unix__
	std::array<char, BUFFER_LEN> buffer{};
	auto res = pthread_getname_np(thread, buffer.data(), buffer.size());
	
	if(res) {
		throw std::runtime_error("Unable to get thread name, error code" + std::to_string(res));
	}

	return std::wstring(buffer.data(), buffer.data() + strlen(buffer.data()));
#else
	return L"unsupported"; // default, not implemented
#endif
}

std::wstring get_name(std::jthread& thread) {
#ifndef TARGET_OS_MAC
	const auto handle = thread.native_handle();
	return get_name(handle);
#else
	// not implemented, MacOS POSIX functions don't take a thread arg (might for getter, not sure)
	return L"unsupported";
#endif
}

std::wstring get_name(std::thread& thread) {
#ifndef TARGET_OS_MAC
	const auto handle = thread.native_handle();
	return get_name(handle);
#else
	// not implemented, MacOS POSIX functions don't take a thread arg (might for getter, not sure)
	return L"unsupported";
#endif
}

std::wstring get_name() {
#ifdef _WIN32
	auto handle = GetCurrentThread();
	return get_name(handle);
#elif defined __linux__ || defined __unix__ || TARGET_OS_MAC
	auto handle = pthread_self();
	return get_name(handle);
#else
#pragma message WARN("Getting thread names is not implemented for this platform. Implement it, please!");
	return get_name(0); // just return a default, empty string
#endif
}

} // thread, ember