/*
 * Copyright (c) 2014 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/util/StringHash.h>
#include <boost/unordered/unordered_flat_map.hpp>
#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <cstdint>

namespace sql {

class Driver;
class Connection;
class PreparedStatement;

} // sql

namespace ember::drivers {

struct StatementDeleterWrapper {
	void operator()(sql::PreparedStatement* stmt);
};

class MySQL final {
	using UniqueStmt = std::unique_ptr<sql::PreparedStatement, StatementDeleterWrapper>;

	using QueryCache = boost::unordered_flat_map<std::string, UniqueStmt,
		StringHash, std::equal_to<>>;

	const std::string dsn, database, username, password;
	sql::Driver* driver;
	mutable boost::unordered_flat_map<const sql::Connection*, QueryCache> cache_;
	mutable std::mutex cache_lock_;

	QueryCache* locate_cache(const sql::Connection* conn) const;
	void close_cache(const sql::Connection* conn) const;

public:
	using ConnectionType = sql::Connection*;

	MySQL(std::string user, std::string password, std::string_view host, std::uint16_t port,
	      std::string db = "");

	MySQL(MySQL&& rhs) noexcept
		: dsn(rhs.dsn),
		  database(rhs.database),
	      username(rhs.username),
		  password(rhs.password),
	      cache_(std::move(rhs.cache_)), driver(rhs.driver) { }

	MySQL& operator=(MySQL&&) = delete;
	MySQL& operator=(MySQL&) = delete;
	MySQL(MySQL&) = delete;

	static std::string name();
	static std::string version();

	sql::Connection* open() const;
	bool clean(sql::Connection* conn) const;
	void close(sql::Connection* conn) const;
	bool keep_alive(sql::Connection* conn) const;
	void thread_enter() const;
	void thread_exit() const;
	sql::PreparedStatement* prepare_cached(sql::Connection* conn, std::string key);
	sql::PreparedStatement* prepare_cached(sql::Connection* conn, std::string_view key);
	sql::PreparedStatement* lookup_statement(const sql::Connection* conn, std::string_view key);
	void cache_statement(const sql::Connection* conn, std::string key, UniqueStmt value);
};

} // drivers, ember