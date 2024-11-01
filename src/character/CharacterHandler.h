/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <Character_generated.h>
#include <dbcreader/Storage.h>
#include <protocol/ResultCodes.h>
#include <shared/database/daos/CharacterDAO.h>
#include <shared/util/PCREHelper.h>
#include <shared/util/UTF8.h>
#include <logger/LoggerFwd.h>
//#include <boost/locale.hpp>
#include <pcre.h>
#include <locale>
#include <functional>
#include <string>
#include <optional>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace ember {

class ThreadPool;

class CharacterHandler final {
	using ResultCB = std::function<void(protocol::Result)>;
	using RenameCB = std::function<void(protocol::Result, std::optional<ember::Character>)>;
	using EnumResultCB = std::function<void(bool, std::vector<ember::Character>)>;

	const std::size_t MAX_NAME_LENGTH = 12;
	const std::size_t MIN_NAME_LENGTH = 2;
	const std::size_t MAX_CONSECUTIVE_LETTERS = 2;
	const std::size_t MAX_CHARACTER_SLOTS_SERVER = 10;
	const std::size_t MAX_CHARACTER_SLOTS_ACCOUNT = 100; // todo, allow config

	const std::vector<util::pcre::Result> profane_names_;
	const std::vector<util::pcre::Result> reserved_names_;
	const std::vector<util::pcre::Result> spam_names_;
	const dbc::Storage& dbc_;
	const dal::CharacterDAO& dao_;
	const std::locale locale_;

	ThreadPool& pool_;
	log::Logger& logger_;

	protocol::Result validate_name(const utf8_string& name) const;
	bool validate_options(const ember::Character& character, std::uint32_t account_id) const;
	void populate_items(ember::Character& character, const dbc::CharStartOutfit& outfit) const;
	void populate_spells(ember::Character& character, const dbc::CharStartSpells& spells) const;
	void populate_skills(ember::Character& character, const dbc::CharStartSkills& skills) const;
	const dbc::FactionGroup* pvp_faction(const dbc::FactionTemplate& fac_template) const;

	/** I/O heavy functions run async in a thread pool **/

	void do_create(std::uint32_t account_id,
	               std::uint32_t realm_id,
	               ember::Character character,
	               const ResultCB& callback) const;

	void do_erase(std::uint32_t account_id,
	              std::uint32_t realm_id,
	              std::uint64_t character_id,
	              const ResultCB& callback) const;

	void do_enumerate(std::uint32_t account_id,
	                  std::uint32_t realm_id,
	                  const EnumResultCB& callback) const;

	void do_rename(std::uint32_t account_id,
	               std::uint64_t character_id,
	               const utf8_string& name,
	               const RenameCB& callback) const;

	void do_restore(std::uint64_t id,
	                const ResultCB& callback) const;


public:
	CharacterHandler(std::vector<util::pcre::Result> profane_names,
	                 std::vector<util::pcre::Result> reserved_names,
	                 std::vector<util::pcre::Result> spam_names,
	                 const dbc::Storage& dbc, const dal::CharacterDAO& dao,
	                 ThreadPool& pool, const std::locale& locale, log::Logger& logger)
		: profane_names_(std::move(profane_names)),
		  reserved_names_(std::move(reserved_names)),
		  spam_names_(std::move(spam_names)),
		  dbc_(dbc),
		  dao_(dao),
		  pool_(pool),
		  locale_(locale),
		  logger_(logger) {}

	void create(std::uint32_t account_id,
	            std::uint32_t realm_id,
	            const rpc::Character::CharacterTemplate& options,
	            ResultCB callback) const;

	void erase(std::uint32_t account_id,
	           std::uint32_t realm_id,
	           std::uint64_t character_id,
	           ResultCB callback) const;

	void enumerate(std::uint32_t account_id,
	               std::uint32_t realm_id,
	               EnumResultCB callback) const;

	void rename(std::uint32_t account_id,
	            std::uint64_t character_id,
	            const utf8_string& name,
	            RenameCB callback) const;

	void restore(std::uint64_t id,
	             ResultCB callback) const;
};

} // ember