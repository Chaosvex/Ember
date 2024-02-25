/*
 * Copyright (c) 2015 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/database/daos/shared_base/IPBanBase.h>
#include <conpool/ConnectionPool.h>
#include <mysql_connection.h>
#include <cppconn/exception.h>
#include <conpool/drivers/MySQL/Driver.h>
#include <cppconn/prepared_statement.h>
#include <memory>

namespace ember::dal {

using namespace std::chrono_literals;

template<typename T>
class MySQLIPBanDAO final : public IPBanDAO {
	T& pool_;
	drivers::MySQL* driver_;

public:
	MySQLIPBanDAO(T& pool) : pool_(pool), driver_(pool.get_driver()) { }

	std::optional<std::uint32_t> get_mask(const std::string& ip) const override try {
		const std::string query = "SELECT cidr FROM ip_bans WHERE ip = ?";

		auto conn = pool_.try_acquire_for(60s);
		sql::PreparedStatement* stmt = driver_->prepare_cached(*conn, query);
		stmt->setString(1, ip);
		std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());

		if(res->next()) {
			return res->getUInt("cidr");
		}

		return std::nullopt;
	} catch(const std::exception& e) {
		throw exception(e.what());
	}

	std::vector<IPEntry> all_bans() const override try {
		const std::string query = "SELECT ip, cidr FROM ip_bans";

		auto conn = pool_.try_acquire_for(60s);
		sql::PreparedStatement* stmt = driver_->prepare_cached(*conn, query);
		std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
		std::vector<IPEntry> entries;

		while(res->next()) {
			entries.emplace_back(res->getString("ip"), res->getUInt("cidr"));
		}

		return entries;
	} catch(const std::exception& e) {
		throw exception(e.what());
	}

	void ban(const IPEntry& ban) const override try {
		const std::string query = "INSERT INTO ip_bans (ip, cidr) VALUES (?, ?)";

		auto conn = pool_.try_acquire_for(60s);
		sql::PreparedStatement* stmt = driver_->prepare_cached(*conn, query);
		stmt->setString(1, ban.first);
		stmt->setUInt(2, ban.second);
		stmt->executeQuery();
	} catch(const std::exception& e) {
		throw exception(e.what());
	}
};

template<typename T>
std::unique_ptr<MySQLIPBanDAO<T>> ip_ban_dao(T& pool) {
	return std::make_unique<MySQLIPBanDAO<T>>(pool);
}

} //dal, ember
