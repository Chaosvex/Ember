/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <CharacterServiceStub.h>
#include <protocol/ResultCodes.h>
#include <shared/database/objects/Character.h>
#include <span>

namespace ember {

class CharacterHandler;

class CharacterService final : public services::CharacterService {
	const CharacterHandler& handler_;
	log::Logger& logger_;

	void send_rename(const protocol::Result& res,
	                 const std::optional<ember::Character>& character,
	                 const spark::Link& link,
	                 const spark::Token& token) const;

	void send_characters(bool result,
	                     std::span<const ember::Character> characters,
	                     const spark::Link& link,
	                     const spark::Token& token) const;

	std::optional<rpc::Character::CreateResponseT> handle_create(
		const rpc::Character::Create& msg,
		const spark::Link& link,
		const spark::Token& token) override;

	std::optional<rpc::Character::DeleteResponseT> handle_delete(
		const rpc::Character::Delete& msg,
		const spark::Link& link,
		const spark::Token& token) override;

	std::optional<rpc::Character::RenameResponseT> handle_rename(
		const rpc::Character::Rename& msg,
		const spark::Link& link,
		const spark::Token& token) override;

	std::optional<rpc::Character::RetrieveResponseT> handle_enumerate(
		const rpc::Character::Retrieve& msg,
		const spark::Link& link,
		const spark::Token& token) override;

	void on_link_up(const spark::Link& link) override;
	void on_link_down(const spark::Link& link) override;

public:
	CharacterService(spark::Server& spark, const CharacterHandler& handler, log::Logger& logger);
};

} // ember