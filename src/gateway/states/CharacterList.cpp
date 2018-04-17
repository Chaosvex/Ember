/*
 * Copyright (c) 2016 - 2018 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "CharacterList.h"
#include "Account_generated.h"
#include "../Config.h"
#include "../Locator.h"
#include "../ClientHandler.h"
#include "../RealmQueue.h"
#include "../CharacterService.h"
#include "../ClientConnection.h"
#include "../EventDispatcher.h"
#include "../FilterTypes.h"
#include <logger/Logging.h>
#include <protocol/Packets.h>
#include <protocol/Opcodes.h>
#include <memory>
#include <vector>

namespace em = ember::messaging;

namespace ember::character_list {

namespace {

void handle_timeout(ClientContext* ctx);

void send_character_list_fail(ClientContext* ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << __func__ << LOG_ASYNC;

	protocol::SMSG_CHAR_CREATE response;
	response->result = protocol::Result::AUTH_UNAVAILABLE;
	ctx->connection->send(response);
}

void send_character_list(ClientContext* ctx, std::vector<Character> characters) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << __func__ << LOG_ASYNC;

	// emulate a quirk of the retail server
	if(Locator::config()->list_zone_hide) {
		for(auto& c : characters) {
			if(c.first_login) {
				c.zone = 0;
			}
		}
	}

	protocol::SMSG_CHAR_ENUM response;
	response->characters = std::move(characters);
	ctx->connection->send(response);
}

void send_character_rename(ClientContext* ctx, protocol::Result result,
                           std::uint64_t id = 0, const std::string& name = "") {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << __func__ << LOG_ASYNC;

	protocol::SMSG_CHAR_RENAME response;
	response->result = result;
	response->id = id;
	response->name = name;
	ctx->connection->send(response);
}

void character_rename(ClientContext* ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << __func__ << LOG_ASYNC;

	protocol::CMSG_CHAR_RENAME packet;

	if(!ctx->handler->packet_deserialise(packet, *ctx->buffer)) {
		return;
	}

	const auto uuid = ctx->handler->uuid();

	Locator::character()->rename_character(ctx->account_id, packet->id, packet->name,
	                                       [uuid](auto status, auto result,
	                                              auto id, const auto& name) {
		auto event = std::make_unique<CharRenameResponse>(status, result, id, name);
		Locator::dispatcher()->post_event(uuid, std::move(event));
	});
}

void character_rename_completion(ClientContext* ctx, const CharRenameResponse* event) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << __func__ << LOG_ASYNC;

	protocol::Result result = protocol::Result::CHAR_NAME_FAILURE;

	if(event->status == em::character::Status::OK) {
		result = event->result;
	}

	if(result == protocol::Result::RESPONSE_SUCCESS) {
		send_character_rename(ctx, result, event->character_id, event->name);
	} else {
		send_character_rename(ctx, result);
	}
}

void character_enumerate(ClientContext* ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << __func__ << LOG_ASYNC;

	const auto uuid = ctx->handler->uuid();

	Locator::character()->retrieve_characters(ctx->account_id,
	                                          [uuid](auto status, auto characters) {
		auto event = std::make_unique<CharEnumResponse>(status, std::move(characters));
		Locator::dispatcher()->post_event(uuid, std::move(event));
	});
}

void character_enumerate_completion(ClientContext* ctx, const CharEnumResponse* event) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << __func__ << LOG_ASYNC;

	if(event->status == em::character::Status::OK) {
		send_character_list(ctx, event->characters);
	} else {
		send_character_list_fail(ctx);
	}
}

void send_character_delete(ClientContext* ctx, protocol::Result result) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << __func__ << LOG_ASYNC;

	protocol::SMSG_CHAR_DELETE response;
	response->result = result;
	ctx->connection->send(response);
}

void send_character_create(ClientContext* ctx, protocol::Result result) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << __func__ << LOG_ASYNC;

	protocol::SMSG_CHAR_CREATE response;
	response->result = result;
	ctx->connection->send(response);
}

void character_create(ClientContext* ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << __func__ << LOG_ASYNC;

	protocol::CMSG_CHAR_CREATE packet;

	if(!ctx->handler->packet_deserialise(packet, *ctx->buffer)) {
		return;
	}

	const auto uuid = ctx->handler->uuid();

	Locator::character()->create_character(ctx->account_id, packet->character,
	                                       [uuid](auto status, auto result) {
		Locator::dispatcher()->post_event(uuid, CharCreateResponse(status, result));
	});
}

void character_create_completion(ClientContext* ctx, const CharCreateResponse* event) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << __func__ << LOG_ASYNC;

	if(event->status == em::character::Status::OK) {
		send_character_create(ctx, event->result);
	} else {
		send_character_create(ctx, protocol::Result::CHAR_CREATE_ERROR);
	}
}

void character_delete(ClientContext* ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << __func__ << LOG_ASYNC;

	protocol::CMSG_CHAR_DELETE packet;

	if(!ctx->handler->packet_deserialise(packet, *ctx->buffer)) {
		return;
	}

	const auto uuid = ctx->handler->uuid();

	Locator::character()->delete_character(ctx->account_id, packet->id,
	                                       [uuid](auto status, auto result) {
		Locator::dispatcher()->post_event(uuid, CharDeleteResponse(status, result));
	});
}

void character_delete_completion(ClientContext* ctx, const CharDeleteResponse* event) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << __func__ << LOG_ASYNC;

	if(event->status == em::character::Status::OK) {
		send_character_delete(ctx, event->result);
	} else {
		send_character_delete(ctx, protocol::Result::CHAR_DELETE_FAILED);
	}
}

void player_login(ClientContext* ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << __func__ << LOG_ASYNC;

	ctx->handler->state_update(ClientState::IN_WORLD);

	protocol::CMSG_PLAYER_LOGIN packet;

	if(!ctx->handler->packet_deserialise(packet, *ctx->buffer)) {
		return;
	}
}

void handle_timeout(ClientContext* ctx) {
	LOG_DEBUG_GLOB << "Character list timed out for "
		<< ctx->connection->remote_address() << LOG_ASYNC;
	ctx->connection->close_session();
}

} // unnamed

void enter(ClientContext* ctx) {
	ctx->handler->start_timer(CHAR_LIST_TIMEOUT);
}

void handle_packet(ClientContext* ctx, protocol::ClientOpcode opcode) {
	switch(opcode) {
		case protocol::ClientOpcode::CMSG_CHAR_ENUM:
			character_enumerate(ctx);
			break;
		case protocol::ClientOpcode::CMSG_CHAR_CREATE:
			character_create(ctx);
			break;
		case protocol::ClientOpcode::CMSG_CHAR_DELETE:
			character_delete(ctx);
			break;
		case protocol::ClientOpcode::CMSG_CHAR_RENAME:
			character_rename(ctx);
			break;
		case protocol::ClientOpcode::CMSG_PLAYER_LOGIN:
			player_login(ctx);
			break;
		default:
			ctx->handler->packet_skip(*ctx->buffer, opcode);
	}
}

void handle_event(ClientContext* ctx, const Event* event) {
	switch(event->type) {
		case EventType::TIMER_EXPIRED:
			handle_timeout(ctx);
			break;
		case EventType::CHAR_CREATE_RESPONSE:
			character_create_completion(ctx, static_cast<const CharCreateResponse*>(event));
			break;
		case EventType::CHAR_DELETE_RESPONSE:
			character_delete_completion(ctx, static_cast<const CharDeleteResponse*>(event));
			break;
		case EventType::CHAR_ENUM_RESPONSE:
			character_enumerate_completion(ctx, static_cast<const CharEnumResponse*>(event));
			break;
		case EventType::CHAR_RENAME_RESPONSE:
			character_rename_completion(ctx, static_cast<const CharRenameResponse*>(event));
			break;
		default:
			break;
	}
}

void exit(ClientContext* ctx) {
	ctx->handler->stop_timer();

	if(ctx->state == ClientState::SESSION_CLOSED) {
		//--test;
		Locator::queue()->free_slot();
	}
}

} // character_list, ember