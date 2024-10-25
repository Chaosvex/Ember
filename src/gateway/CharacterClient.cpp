/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "CharacterClient.h"
#include <memory>

namespace ember {

using namespace rpc::Character;

CharacterClient::CharacterClient(spark::Server& server, Config& config, log::Logger& logger)
	: services::CharacterClient(server),
	  config_(config),
	  logger_(logger) {
	connect("127.0.0.1", 8003); // temp
}

void CharacterClient::on_link_up(const spark::Link& link) {
	LOG_DEBUG_ASYNC(logger_, "Link up: {}", link.peer_banner);
	link_ = link;
}

void CharacterClient::on_link_down(const spark::Link& link) {
	LOG_DEBUG_ASYNC(logger_, "Link down: {}", link.peer_banner);
}

void CharacterClient::connect_failed(std::string_view ip, const std::uint16_t port) {
	LOG_INFO_ASYNC(logger_, "Failed to connect to character service on {}:{}", ip, port);
}

void CharacterClient::retrieve_characters(const std::uint32_t account_id, RetrieveCB cb) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	RetrieveT msg {
		.account_id = account_id,
		.realm_id = config_.realm->id
	};

	send<RetrieveResponse>(msg, link_, [this, cb](auto link, auto message) {
		handle_retrieve_reply(link, message, cb);
	});
}

void CharacterClient::create_character(const std::uint32_t account_id,
									   const CharacterTemplateT& character,
									   ResponseCB cb) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	CreateT msg;
	msg.account_id = account_id;
	msg.realm_id = config_.realm->id;
	msg.character = std::make_unique<CharacterTemplateT>(character);

	send<CreateResponse>(msg, link_, [this, cb](auto link, auto message) {
		handle_create_reply(link, message, cb);
	});
}

void CharacterClient::delete_character(std::uint32_t account_id,
									   std::uint64_t id,
									   ResponseCB cb) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	DeleteT msg {
		.account_id = account_id,
		.realm_id = config_.realm->id,
		.character_id = id,
	};

	send<DeleteResponse>(msg, link_, [this, cb](auto link, auto message) {
		handle_delete_reply(link, message, cb);
	});
}

void CharacterClient::rename_character(std::uint32_t account_id,
									   std::uint64_t character_id,
									   const utf8_string& name,
									   RenameCB cb) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	RenameT msg {
		.account_id = account_id,
		.name = name,
		.realm_id = config_.realm->id,
		.character_id = character_id,
	};

	send<RenameResponse>(msg, link_, [this, cb](auto link, auto message) {
		handle_rename_reply(link, message, cb);
	});
}

void CharacterClient::handle_create_reply(
	const spark::Link& link,
	std::expected<const CreateResponse*, spark::Result> res,
	ResponseCB cb) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	if(!res) {
		cb(protocol::Result::CHAR_CREATE_ERROR);
		return;
	}

	const auto msg = *res;
	cb(protocol::Result(msg->result()));
}

void CharacterClient::handle_rename_reply(
	const spark::Link& link,
	std::expected<const RenameResponse*, spark::Result> res,
	RenameCB cb) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	if(!res) {
		cb(protocol::Result::CHAR_NAME_FAILURE, 0, "");
		return;
	}

	const auto msg = *res;

	if(!msg->name()) {
		cb(protocol::Result::CHAR_NAME_FAILURE, 0, "");
		return;
	}

	cb(protocol::Result(msg->result()), msg->character_id(), msg->name()->str());
}

void CharacterClient::handle_retrieve_reply(
	const spark::Link& link,
	std::expected<const RetrieveResponse*, spark::Result> res,
	RetrieveCB cb) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	if(!res) {
		cb(Status::UNKNOWN_ERROR, {});
		return;
	}

	const auto msg = *res;

	if(!msg->characters()) {
		cb(Status::OK, {});
		return;
	}

	std::vector<ember::Character> characters;
	characters.reserve(msg->characters()->size());
	auto key = msg->characters();

	for(auto i = key->begin(); i != key->end(); ++i) {
		ember::Character character;
		character.name = i->name()->str();
		character.id = i->id();
		character.account_id = i->account_id();
		character.realm_id = i->realm_id();
		character.race = i->race();
		character.class_ = i->class_();
		character.gender = i->gender();
		character.skin = i->skin();
		character.face = i->face();
		character.hairstyle = i->hairstyle();
		character.haircolour = i->haircolour();
		character.facialhair = i->facialhair();
		character.level = i->level();
		character.zone = i->zone();
		character.map = i->map();
		character.guild_id = i->guild_id();
		character.guild_rank = i->guild_rank();
		character.position.x = i->x();
		character.position.y = i->y();
		character.position.z = i->z();
		character.orientation = i->orientation();
		character.flags = static_cast<ember::Character::Flags>(i->flags());
		character.first_login = i->first_login();
		character.pet_display = i->pet_display_id();
		character.pet_level = i->pet_level();
		character.pet_family = i->pet_family();
		characters.emplace_back(std::move(character));
	}

	cb(Status::OK, std::move(characters));
}

void CharacterClient::handle_delete_reply(
	const spark::Link& link,
	std::expected<const DeleteResponse*, spark::Result> res,
	ResponseCB cb) const {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	if(!res) {
		cb(protocol::Result::CHAR_DELETE_FAILED);
		return;
	}

	const auto msg = *res;
	LOG_TRACE_ASYNC(logger_, "Result: {}", msg->result());
	cb(protocol::Result(msg->result()));
}

} // ember