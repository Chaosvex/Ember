/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Sessions.h"

namespace ember {

bool Sessions::register_session(std::uint32_t account_id, const Botan::BigInt& key) {
	std::lock_guard guard(lock_);

	auto it = sessions_.find(account_id);

	if(!allow_overwrite_ && it != sessions_.end()) {
		return false;
	}

	sessions_.insert_or_assign(account_id, key);
	return true;
}

std::optional<Botan::BigInt> Sessions::lookup_session(std::uint32_t account_id) const {
	std::lock_guard guard(lock_);
	auto it = sessions_.find(account_id);

	if(it == sessions_.end()) {
		return std::nullopt;
	}

	return std::optional<Botan::BigInt>(it->second);
}

} // ember