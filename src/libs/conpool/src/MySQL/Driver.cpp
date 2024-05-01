/*
 * Copyright (c) 2014 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <conpool/drivers/MySQL/Driver.h>
#include <mysql_driver.h>
#include <cppconn/driver.h>
#include <cppconn/connection.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <sstream>

namespace ember::drivers {

MySQL::MySQL(std::string user, std::string pass, const std::string& host, unsigned short port,
             std::string db) : database(std::move(db)), username(std::move(user)),
             password(std::move(pass)),
             dsn(std::string("tcp://" + host + ":" + std::to_string(port))) {
	driver = get_driver_instance();
}

sql::Connection* MySQL::open() const {
	sql::Connection* conn = driver->connect(dsn, username, password);

	if(!database.empty()) {
		conn->setSchema(database);
	}

	conn->setAutoCommit(true);
	bool opt = true;
	conn->setClientOption("MYSQL_OPT_RECONNECT", &opt);
	return conn;
}

void MySQL::close(sql::Connection* conn) const {
	if(!conn->isClosed()) {
		conn->close();
	}

	auto conn_cache = locate_cache(conn);

	if(conn_cache) {
		close_cache(conn);
	}

	delete conn;
}

bool MySQL::keep_alive(sql::Connection* conn) const try {
	std::unique_ptr<sql::Statement> stmt(conn->createStatement());
	stmt->execute("/* ping */");
	return true;
} catch(sql::SQLException&) {
	return false;
}

bool MySQL::clean(sql::Connection* conn) const try {
	return conn->isValid();
} catch(sql::SQLException&) {
	return false;
}

void MySQL::thread_enter() const {
	driver->threadInit();
}

void MySQL::thread_exit() const {
	driver->threadEnd();
}

std::string MySQL::name() {
	auto driver = get_driver_instance();
	return driver->getName();
}

std::string MySQL::version() {
	auto driver = get_driver_instance();
	std::stringstream ver;
	ver << driver->getMajorVersion() << "." << driver->getMinorVersion() << "."
	    << driver->getPatchVersion();
	return ver.str();
}

sql::PreparedStatement* MySQL::prepare_cached(sql::Connection* conn, std::string_view key) {
	auto stmt = lookup_statement(conn, key);
	
	if(!stmt) {
		stmt = conn->prepareStatement(std::string(key));
		cache_statement(conn, key, stmt);
	}

	return stmt;
}

sql::PreparedStatement* MySQL::lookup_statement(const sql::Connection* conn, std::string_view key) {
	auto conn_cache = locate_cache(conn);

	if(!conn_cache) {
		return nullptr;
	}

	auto conn_cache_it = conn_cache->find(key);
	
	if(conn_cache_it == conn_cache->end()) {
		return nullptr;
	}

	return conn_cache_it->second.get();
}

void MySQL::cache_statement(const sql::Connection* conn, std::string_view key,
                            sql::PreparedStatement* value) {
	std::lock_guard<std::mutex> lock(cache_lock_);
	auto ptr = stmt_ptr(value, [](auto stmt) { stmt->close(); });
	cache_[conn].emplace(key, std::move(ptr));
}

MySQL::QueryCache* MySQL::locate_cache(const sql::Connection* conn) const {
	std::lock_guard<std::mutex> lock(cache_lock_);
	auto cache_it = cache_.find(conn);

	if(cache_it == cache_.end()) {
		return nullptr;
	}

	return &cache_it->second;
}

void MySQL::close_cache(const sql::Connection* conn) const {
	// todo - iterators (ex. erased element) not invalidated by erase, research
	std::lock_guard<std::mutex> lock(cache_lock_);
	cache_.erase(conn);
}

} // drivers, ember