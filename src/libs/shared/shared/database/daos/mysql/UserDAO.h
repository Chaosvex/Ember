/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/database/daos/shared_base/UserBase.h>
#include <conpool/ConnectionPool.h>
#include <mysql_connection.h>
#include <cppconn/exception.h>
#include <conpool/drivers/MySQL/Driver.h>
#include <cppconn/prepared_statement.h>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>

namespace ember::dal { 

using namespace std::chrono_literals;

template<typename T>
class MySQLUserDAO final : public UserDAO {
	T& pool_;
	drivers::MySQL* driver_;

public:
	MySQLUserDAO(T& pool) : pool_(pool), driver_(pool.get_driver()) { }

	std::optional<User> user(const std::string& username) const override try {
		std::string_view query = "SELECT u.username, u.id, u.s, u.v, u.pin_method, u.pin, "
		                         "u.totp_key, b.user_id as banned, u.survey_request, u.subscriber, u.verified "
		                         "s.user_id as suspended FROM users u "
		                         "LEFT JOIN bans b ON u.id = b.user_id "
		                         "LEFT JOIN suspensions s ON u.id = s.user_id "
		                         "WHERE username = ?";

		auto conn = pool_.try_acquire_for(5s);
		sql::PreparedStatement* stmt = driver_->prepare_cached(*conn, query);
		stmt->setString(1, username);
		std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());

		if(res->next()) {
			auto salt_it = res->getBlob("s");
			std::vector<std::uint8_t> salt((std::istreambuf_iterator<char>(*salt_it)),
				std::istreambuf_iterator<char>());

			User user(res->getUInt("id"), res->getString("username"), std::move(salt),
			          res->getString("v"), static_cast<PINMethod>(res->getUInt("pin_method")),
			          res->getUInt("pin"), res->getString("totp_key"), res->getBoolean("banned"),
			          res->getBoolean("suspended"), res->getBoolean("survey_request"),
			          res->getBoolean("subscriber"), res->getBoolean("verified"));
			return user;
		}

		return std::nullopt;
	} catch(const std::exception& e) {
		throw exception(e.what());
	}

	void save_survey(std::uint32_t account_id, std::uint32_t survey_id,
	                 const std::string& data) const override try {
		auto conn = pool_.try_acquire_for(5s);
		conn->setAutoCommit(false);

		try {
			// intentionally not storing the user ID with the survey data, not an oversight :)
			std::string_view query = "INSERT INTO survey_results (survey_id, data) VALUES (?, ?)";

			sql::PreparedStatement* stmt = driver_->prepare_cached(*conn, query);
			stmt->setUInt(1, survey_id);
			stmt->setString(2, data);
	
			if(!stmt->executeUpdate()) {
				throw exception("Unable to save survey data for account ID " + std::to_string(account_id));
			}

			query = "UPDATE users SET survey_request = 0 WHERE id = ?";

			stmt = driver_->prepare_cached(*conn, query);
			stmt->setUInt(1, account_id);

			if(!stmt->executeUpdate()) {
				throw exception("Unable to save survey data for account ID " + std::to_string(account_id));
			}

			conn->commit();
		} catch(const std::exception& e) {
			conn->rollback();
			conn->setAutoCommit(true);
			throw exception(e.what());
		}

		conn->setAutoCommit(true);
	} catch(const std::exception& e) {
		throw exception(e.what());
	}

	void record_last_login(std::uint32_t account_id, const std::string& ip) const override try {
		std::string_view query = "INSERT INTO login_history (user_id, ip) VALUES "
		                         "((SELECT id AS user_id FROM users WHERE id = ?), ?)";

		auto conn = pool_.try_acquire_for(5s);
		sql::PreparedStatement* stmt = driver_->prepare_cached(*conn, query);
		stmt->setUInt(1, account_id);
		stmt->setString(2, ip);
		
		if(!stmt->executeUpdate()) {
			throw exception("Unable to set last login for account ID " + std::to_string(account_id));
		}
	} catch(const std::exception& e) {
		throw exception(e.what());
	}

	std::unordered_map<std::uint32_t, std::uint32_t>
	character_counts(std::uint32_t account_id) const override try {
		std::string_view query = "SELECT COUNT(c.id) AS count, c.realm_id "
		                         "FROM users u, characters c "
		                         "WHERE u.id = ? AND c.deletion_date IS NULL "
		                         "GROUP BY c.realm_id";
		
		auto conn = pool_.try_acquire_for(5s);
		sql::PreparedStatement* stmt = driver_->prepare_cached(*conn, query);
		stmt->setUInt(1, account_id);
		std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());

		std::unordered_map<std::uint32_t, std::uint32_t> counts;

		if(res->next()) {
			counts.emplace(res->getUInt("realm_id"), res->getUInt("count"));
		}

		return counts;
	} catch(const std::exception& e) {
		throw exception(e.what());
	}
};

template<typename T>
MySQLUserDAO<T> user_dao(T& pool) {
	return MySQLUserDAO<T>(pool);
}

} // dal, ember