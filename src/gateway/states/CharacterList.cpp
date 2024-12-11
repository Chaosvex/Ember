/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "CharacterList.h"
#include "../Config.h"
#include "../Locator.h"
#include "../ClientHandler.h"
#include "../RealmQueue.h"
#include "../CharacterClient.h"
#include "../ClientConnection.h"
#include "../EventDispatcher.h"
#include "../FilterTypes.h"
#include "../ClientLogHelper.h"
#include <logger/Logger.h>
#include <protocol/Packets.h>
#include <protocol/Opcodes.h>
#include <shared/util/UTF8String.h>
#include <memory>
#include <vector>

namespace ember::gateway::character_list {

namespace {

void handle_timeout(ClientContext& ctx);

void send_character_list_fail(ClientContext& ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	// displays an error dialogue on the client
	protocol::SMSG_CHAR_CREATE response;
	response->result = protocol::Result::CHAR_LIST_FAILED;
	ctx.connection->send(response);
}

void send_character_list(ClientContext& ctx, std::vector<Character> characters) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

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
	ctx.connection->send(response);
}

void send_character_rename(ClientContext& ctx, const CharRenameResponse* res) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	protocol::SMSG_CHAR_RENAME response;
	response->result = res->result;
	response->id = res->character_id;
	response->name = res->name;
	ctx.connection->send(response);
}

void character_rename(ClientContext& ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	protocol::CMSG_CHAR_RENAME packet;

	if(!ctx.handler->deserialise(packet, *ctx.stream)) {
		return;
	}

	const auto& uuid = ctx.handler->uuid();

	Locator::character()->rename_character(ctx.client_id->id, packet->id, packet->name,
	                                       [uuid](auto result, auto id, const auto& name) {
		CharRenameResponse event(result, id, name);
		Locator::dispatcher()->post_event(uuid, std::move(event));
	});
}

void character_enumerate(const ClientContext& ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	const auto& uuid = ctx.handler->uuid();
	Locator::character()->retrieve_characters(ctx.client_id->id,
		[uuid](auto status, auto characters) {
			CharEnumResponse event(status, std::move(characters));
			Locator::dispatcher()->post_event(uuid, std::move(event));
		}
	);
}

void character_enumerate_completion(ClientContext& ctx, const CharEnumResponse* event) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	if(event->status == rpc::Character::Status::OK) {
		send_character_list(ctx, event->characters);
	} else {
		send_character_list_fail(ctx);
	}
}

void send_character_delete(ClientContext& ctx, const CharDeleteResponse* res) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	protocol::SMSG_CHAR_DELETE response;
	response->result = res->result;
	ctx.connection->send(response);
}

void send_character_create(ClientContext& ctx, const CharCreateResponse* res) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	protocol::SMSG_CHAR_CREATE response;
	response->result = res->result;
	ctx.connection->send(response);
}

void character_create(ClientContext& ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	protocol::CMSG_CHAR_CREATE packet;

	if(!ctx.handler->deserialise(packet, *ctx.stream)) {
		return;
	}

	const auto& uuid = ctx.handler->uuid();

	Locator::character()->create_character(ctx.client_id->id, packet->character, [uuid](auto result) {
		Locator::dispatcher()->post_event(uuid, CharCreateResponse(result));
	});
}

void character_delete(ClientContext& ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	protocol::CMSG_CHAR_DELETE packet;

	if(!ctx.handler->deserialise(packet, *ctx.stream)) {
		return;
	}

	const auto& uuid = ctx.handler->uuid();

	Locator::character()->delete_character(ctx.client_id->id, packet->id, [uuid](auto result) {
		Locator::dispatcher()->post_event(uuid, CharDeleteResponse(result));
	});
}

void player_login(ClientContext& ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	ctx.handler->state_update(ClientState::WORLD_ENTER);

	protocol::CMSG_PLAYER_LOGIN packet;

	if(!ctx.handler->deserialise(packet, *ctx.stream)) {
		return;
	}

	Locator::dispatcher()->post_event(
		ctx.handler->uuid(), PlayerLogin(packet->character_id)
	);

	ctx.handler->state_update(ClientState::WORLD_ENTER);
}

void handle_timeout(ClientContext& ctx) {
	CLIENT_DEBUG_GLOB(ctx) << "Character list timed out" << LOG_ASYNC;
	ctx.handler->close();
}

} // unnamed

void enter(ClientContext& ctx) {
	ctx.handler->start_timer(CHAR_LIST_TIMEOUT);
}

void handle_packet(ClientContext& ctx, protocol::ClientOpcode opcode) {
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
			ctx.handler->skip(*ctx.stream);
	}
}

void handle_event(ClientContext& ctx, const Event* event) {
	switch(event->type) {
		case EventType::TIMER_EXPIRED:
			handle_timeout(ctx);
			break;
		case EventType::CHAR_CREATE_RESPONSE:
			send_character_create(ctx, static_cast<const CharCreateResponse*>(event));
			break;
		case EventType::CHAR_DELETE_RESPONSE:
			send_character_delete(ctx, static_cast<const CharDeleteResponse*>(event));
			break;
		case EventType::CHAR_ENUM_RESPONSE:
			character_enumerate_completion(ctx, static_cast<const CharEnumResponse*>(event));
			break;
		case EventType::CHAR_RENAME_RESPONSE:
			send_character_rename(ctx, static_cast<const CharRenameResponse*>(event));
			break;
		default:
			break;
	}
}

void exit(ClientContext& ctx) {
	ctx.handler->stop_timer();

	if(ctx.state == ClientState::SESSION_CLOSED) {
		//--test;
		Locator::queue()->free_slot();
	}
}

} // character_list, gateway, ember