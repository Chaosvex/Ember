/*
 * Copyright (c) 2014 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <utility>

namespace ember::connection_pool {

namespace sc = std::chrono;
using namespace std::chrono_literals;

template<typename ConType>
struct ConnDetail {
	ConType conn{};
	sc::seconds idle = 0s;
	unsigned int id = 0;
	bool empty_slot   : 1 = true;
	bool dirty        : 1 = false;
	bool checked_out  : 1 = false;
	bool error        : 1 = false;
	bool sweep        : 1 = false;
	bool refresh      : 1 = false;

	ConnDetail(const ConType& connection, unsigned int id)
		: conn(connection),
		  id(id),
		  empty_slot(false) {}

	ConnDetail() = default;
};

template<typename ConType>
class Connection final {
	using ReleaseHandler = std::function<void(Connection<ConType>&)>;

	ReleaseHandler rh_;
	bool released_ = false;
	std::mutex close_protect_;
	mutable std::reference_wrapper<ConnDetail<ConType>> detail_;

	Connection(ReleaseHandler handler, ConnDetail<ConType>& detail)
	           : rh_(std::move(handler)), detail_(detail) {}

public:
	~Connection() {
		if(!released_) {
			rh_(*this);
		}
	}

	Connection(Connection<ConType>&& src) noexcept
	           : detail_(std::move(src.detail_)), rh_(std::move(src.rh_)) {
		src.released_ = true;
	}

	Connection<ConType>& operator=(Connection<ConType>&& src) noexcept {
		if(!released_) {
			rh_(*this);
		}

		released_ = src.released_;
		detail_ = src.detail_;
		return *this;
	}

	void release() {
		std::lock_guard guard(close_protect_);

		if(!released_) {
			released_ = true;
			rh_(*this);
		}
	}

	Connection(const Connection<ConType>& src) = delete;
	Connection<ConType>& operator=(const Connection<ConType>& src) = delete;

	ConType operator->() { return detail_.get().conn; }
	ConType operator*() { return detail_.get().conn; }

	template<typename A, typename B, typename C, unsigned int> friend class Pool;
};


} // connection_pool, ember