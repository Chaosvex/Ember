/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/util/Utility.h>
#include <shared/util/polyfill/start_lifetime_as>
#include <memory>
#include <new>
#include <utility>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace ember::spark::io {

enum class PagePolicy {
	no_lock, lock
};

namespace {

struct FreeBlock {
	FreeBlock* next;
};

template<std::size_t size>
concept gt_zero = size > 0;

template<typename _ty>
concept gte_freeblock = sizeof(_ty) >= sizeof(FreeBlock);

template<typename _ty, std::size_t _elements, PagePolicy _policy>
requires gt_zero<_elements> && gte_freeblock<_ty>
struct Allocator {
	std::unique_ptr<char[]> storage_;
	FreeBlock* head_ = nullptr;
	std::uintptr_t upper_ = 0;
	std::uintptr_t lower_ = 0;

#ifdef _DEBUG_TLS_BLOCK_ALLOCATOR
	std::size_t storage_active_count = 0;
	std::size_t new_active_count = 0;
	std::size_t total_allocs = 0;
	std::size_t total_deallocs = 0;
#endif

	void link_blocks() {
		auto storage = storage_.get();

		for(std::size_t i = 0; i < _elements; ++i) {
			auto block = std::start_lifetime_as<FreeBlock>(storage + (sizeof(_ty) * i));
			block->next = reinterpret_cast<FreeBlock*>(storage + (sizeof(_ty) * (i + 1)));
		}

		auto tail = reinterpret_cast<FreeBlock*>(storage + (sizeof(_ty) * (_elements - 1)));
		tail->next = nullptr;
		head_ = reinterpret_cast<FreeBlock*>(storage);
	}

	void add_block(FreeBlock* block) {
		assert(block);
		block->next = head_;
		head_ = block;
	}

	void remove_block(const FreeBlock* block) {
		assert(block);
		head_ = block->next;
	}

	template<typename ...Args>
	[[nodiscard]] inline _ty* allocate(Args&&... args) {
		// lazy allocation to prevent every created thread allocating
		if(!storage_) [[unlikely]] {
			storage_ = std::make_unique<char[]>(sizeof(_ty) * _elements);

			if constexpr(_policy == PagePolicy::lock) {
				util::page_lock(storage_.get(), sizeof(_ty) * _elements);
			}

			link_blocks();

			lower_ = reinterpret_cast<std::uintptr_t>(storage_.get());
			upper_ = lower_ + (sizeof(_ty) * _elements);
		}

		auto block = head_;

		if(!block) {
#ifdef _DEBUG_TLS_BLOCK_ALLOCATOR
			++new_active_count;
			++total_allocs;
#endif
			return new _ty(std::forward<Args>(args)...);
		}

#ifdef _DEBUG_TLS_BLOCK_ALLOCATOR
		++storage_active_count;
		++total_allocs;
#endif

		remove_block(block);
		return new (block) _ty(std::forward<Args>(args)...);
	}

	inline void deallocate(_ty* t) {
		const auto t_ptr = reinterpret_cast<std::uintptr_t>(t);

		if(t_ptr >= upper_ || t_ptr < lower_) [[unlikely]] {
#ifdef _DEBUG_TLS_BLOCK_ALLOCATOR
			--new_active_count;
			++total_deallocs;
#endif
			delete t;
			return;
		}

		t->~_ty();

		auto block = std::start_lifetime_as<FreeBlock>(t);
		add_block(block);

#ifdef _DEBUG_TLS_BLOCK_ALLOCATOR
		--storage_active_count;
		++total_deallocs;
#endif
	}

	~Allocator() {
		if constexpr(_policy == PagePolicy::lock) {
			util::page_unlock(storage_.get(), sizeof(_ty) * _elements);
		}

#ifdef _DEBUG_TLS_BLOCK_ALLOCATOR
		assert(!storage_active_count && !new_active_count);
		assert(total_allocs == total_deallocs);
#endif
	}
};

} // unnamed

template<typename _ty, std::size_t _elements, PagePolicy policy = PagePolicy::lock>
struct TLSBlockAllocator final {
#ifdef _DEBUG_TLS_BLOCK_ALLOCATOR
	std::size_t total_allocs = 0;
	std::size_t total_deallocs = 0;
	std::size_t active_allocs = 0;
#endif

	template<typename ...Args>
	[[nodiscard]] inline _ty* allocate(Args&&... args) {
#ifdef _DEBUG_TLS_BLOCK_ALLOCATOR
		++total_allocs;
		++active_allocs;
#endif
		return allocator.allocate(std::forward<Args>(args)...);
	}

	inline void deallocate(_ty* t) {
#ifdef _DEBUG_TLS_BLOCK_ALLOCATOR
		++total_deallocs;
		--active_allocs;
#endif
		allocator.deallocate(t);
	}

#ifdef _DEBUG_TLS_BLOCK_ALLOCATOR
	~TLSBlockAllocator() {
		assert(total_allocs == total_deallocs);
		assert(active_allocs == 0);
	}
#endif

#ifndef _DEBUG_TLS_BLOCK_ALLOCATOR
private:
#endif
	static inline thread_local Allocator<_ty, _elements, policy> allocator;
};

} // io, spark, ember