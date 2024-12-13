/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/database/daos/UserDAO.h>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <cstdint>

namespace ember {

class ThreadPool;

class AccountHandler final {
public:
	// temp. unexpected type
	using LookupCB = std::function<void(std::expected<std::optional<std::uint32_t>, bool>)>;

private:
	dal::UserDAO& user_dao_;
	ThreadPool& pool_;

public:
	AccountHandler(dal::UserDAO& user_dao, ThreadPool& pool);

	std::optional<std::uint32_t> lookup_id(const std::string& username);
	void lookup_id(const std::string& username, LookupCB cb);

};

} // ember