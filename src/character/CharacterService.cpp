/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "CharacterService.h"
#include "CharacterHandler.h"

namespace ember {

using namespace rpc::Character;
using namespace spark;

CharacterService::CharacterService(Server& server, const CharacterHandler& handler, log::Logger& logger)
	: services::CharacterService(server),
	  handler_(handler),
	  logger_(logger) {}

void CharacterService::on_link_up(const Link& link) {
	LOG_DEBUG_ASYNC(logger_, "Link up: {}", link.peer_banner);
}

void CharacterService::on_link_down(const Link& link) {
	LOG_DEBUG_ASYNC(logger_, "Link down: {}", link.peer_banner);
}

std::optional<CreateResponseT>
CharacterService::handle_create(const Create& msg, const Link& link, const Token& token) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	if(!msg.character()) {
		return CreateResponseT {
			.status = Status::ILLFORMED_MESSAGE 
		};
	}

	handler_.create(msg.account_id(), msg.realm_id(), *msg.character(), [=, this](auto res) {
		CreateResponseT msg {
			.status = Status::OK,
			.result = std::to_underlying(res)
		};

		send(msg, link, token);
	});

	return std::nullopt;
}

std::optional<DeleteResponseT>
CharacterService::handle_delete(const Delete& msg, const Link& link, const Token& token) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	handler_.erase(msg.account_id(), msg.realm_id(), msg.character_id(), [=, this](auto res) {
		LOG_DEBUG_ASYNC(logger_, "Deletion response: {}", protocol::to_string(res));

		DeleteResponseT msg {
			.status = Status::OK,
			.result = std::to_underlying(res)
		};

		send(msg, link, token);
	});

	return std::nullopt;
}

std::optional<RenameResponseT> 
CharacterService::handle_rename(const Rename& msg, const Link& link, const Token& token) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	if(!msg.name()) {
		return RenameResponseT{
			.status = Status::ILLFORMED_MESSAGE
		};
	}

	handler_.rename(msg.account_id(), msg.character_id(), msg.name()->str(),
		[=, this](auto res, auto character) {
			send_rename(res, character, link, token);
		}
	);

	return std::nullopt;
}

std::optional<RetrieveResponseT>
CharacterService::handle_enumerate(const Retrieve& msg, const Link& link, const Token& token) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	handler_.enumerate(msg.account_id(), msg.realm_id(), [=, this](auto res, auto characters) {
		send_characters(res, characters, link, token);
	});

	return std::nullopt;
}

void CharacterService::send_rename(const protocol::Result& res,
                                   std::optional<ember::Character>& character,
                                   const Link& link,
                                   const Token& token) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	RenameResponseT response {
		.status = Status::OK,
		.result = std::to_underlying(res),
	};

	if(character) {
		response.name = character->name;
		response.character_id = character->id;
	}

	send(response, link, token);
}

void CharacterService::send_characters(const bool result,
                                       std::span<ember::Character> characters,
                                       const Link& link,
                                       const Token& token) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	// painful
	std::vector<std::unique_ptr<CharacterT>> chars;
	chars.reserve(characters.size());

	for(const auto& character : characters) {
		CharacterT fbchar {
			.id = character.id,
			.name = character.name,
			.race = character.race,
			.class_ = character.class_,
			.gender = character.gender,
			.skin = character.skin,
			.face = character.face,
			.hairstyle = character.hairstyle,
			.haircolour = character.haircolour,
			.facialhair = character.facialhair,
			.level = character.level,
			.zone = character.zone,
			.map = character.map,
			.x = character.position.x,
			.y = character.position.y,
			.z = character.position.z,
			.orientation = character.orientation,
			.guild_id = character.guild_id,
			.flags = static_cast<Flags>(character.flags),
			.first_login = character.first_login,
			.pet_display_id = character.pet_display,
			.pet_level = character.pet_level,
			.pet_family = character.pet_family
		};

		chars.emplace_back(std::make_unique<CharacterT>(fbchar));
	}

	RetrieveResponseT response;

	if(result) {
		response.status = Status::OK;
		response.characters = std::move(chars);
	} else {
		response.status = Status::UNKNOWN_ERROR;
	}

	send(response, link, token);
}

} // ember