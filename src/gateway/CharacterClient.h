/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Config.h"
#include <logger/Logger.h>
#include <CharacterClientStub.h>
#include <protocol/ResultCodes.h>
#include <shared/database/objects/Character.h>

namespace ember::gateway {

class CharacterClient final : public services::CharacterClient {
public:
	using ResponseCB = std::function<void(protocol::Result)>;
	using RenameCB = std::function<void(protocol::Result, std::uint64_t, std::string)>;
	using RetrieveCB = std::function<void(rpc::Character::Status, std::vector<ember::Character>)>;

private:
	const Config& config_;
	spark::Link link_;
	log::Logger& logger_;

	void on_link_up(const spark::Link& link) override;
	void on_link_down(const spark::Link& link) override;
	void connect_failed(std::string_view ip, std::uint16_t port) override;

	void handle_create_reply(
		const spark::Link& link,
		std::expected<const rpc::Character::CreateResponse*, spark::Result> res,
		ResponseCB cb) const;

	void handle_rename_reply(
		const spark::Link& link,
		std::expected<const rpc::Character::RenameResponse*, spark::Result> res,
		RenameCB cb) const;

	void handle_retrieve_reply(
		const spark::Link& link,
		std::expected<const rpc::Character::RetrieveResponse*, spark::Result> res,
		RetrieveCB cb) const;

	void handle_delete_reply(
		const spark::Link& link,
		std::expected<const rpc::Character::DeleteResponse*, spark::Result> res,
		ResponseCB cb) const;

public:
	CharacterClient(spark::Server& server, Config& config, log::Logger& logger);

	void retrieve_characters(std::uint32_t account_id,
	                         RetrieveCB cb) const;

	void create_character(std::uint32_t account_id,
	                      const rpc::Character::CharacterTemplateT& character,
	                      ResponseCB cb) const;

	void delete_character(std::uint32_t account_id,
	                      std::uint64_t id,
	                      ResponseCB cb) const;

	void rename_character(std::uint32_t account_id,
	                      std::uint64_t character_id,
	                      const utf8_string& name,
	                      RenameCB cb) const;
};

} // gateway, ember