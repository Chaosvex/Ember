/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Authentication.h"
#include "../AccountClient.h"
#include "../Config.h"
#include "../RealmQueue.h"
#include "../ClientConnection.h"
#include "../Locator.h"
#include "../EventDispatcher.h"
#include "../ClientLogHelper.h"
#include <protocol/Opcodes.h>
#include <protocol/PacketHeaders.h>
#include <protocol/Packets.h>
#include <spark/buffers/pmr/Buffer.h>
#include <shared/util/EnumHelper.h>
#include <shared/util/UTF8String.h>
#include <shared/util/xoroshiro128plus.h>
#include <logger/Logger.h>
#include <botan/bigint.h>
#include <botan/hash.h>
#include <boost/assert.hpp>
#include <boost/container/small_vector.hpp>
#include <gsl/gsl_util>
#include <utility>
#include <cstddef>
#include <cstdint>

namespace ember::gateway::authentication {

using AddonData = protocol::client::AuthSession::AddonData;

void send_auth_challenge(ClientContext& ctx);
void send_auth_result(ClientContext& ctx, protocol::Result result);
void handle_authentication(ClientContext& ctx);
void handle_queue_update(ClientContext& ctx, const QueuePosition* event);
void handle_queue_success(ClientContext& ctx);
void auth_success(ClientContext& ctx);
void auth_queue(ClientContext& ctx);
void prove_session(ClientContext& ctx, const Botan::BigInt& key);
void fetch_session_key(ClientContext& ctx, std::uint32_t account_id);
void fetch_account_id(ClientContext& ctx, const utf8_string& username);
void handle_timeout(ClientContext& ctx);
void send_addon_data(ClientContext& ctx);

void auth_state(ClientContext& ctx, State state) {
	auto& state_ctx = std::get<Context>(ctx.state_ctx);
	state_ctx.state = state;
}

State auth_state(ClientContext& ctx) {
	const auto& state_ctx = std::get<Context>(ctx.state_ctx);
	return state_ctx.state;
}

void handle_authentication(ClientContext& ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	// prevent repeated auth attempts
	if(auth_state(ctx) != State::NOT_AUTHED) {
		return;
	}

	auto& auth_ctx = std::get<Context>(ctx.state_ctx);

	if(!ctx.handler->deserialise(auth_ctx.packet, *ctx.stream)) {
		return;
	}

	CLIENT_DEBUG_GLOB(ctx)
		<< "Received session proof for "
		<< auth_ctx.packet->username
		<< LOG_ASYNC;
	
	if(auth_ctx.packet->build == 0) {
		// todo - check game build & server ID
	}

	auth_state(ctx, State::IN_PROGRESS);
	fetch_account_id(ctx, auth_ctx.packet->username);
}

void fetch_account_id(ClientContext& ctx, const utf8_string& username) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	const auto& uuid = ctx.handler->uuid();

	Locator::account()->locate_account_id(username, [uuid](auto status, auto id) {
		AccountIDResponse event(status, id);
		Locator::dispatcher()->post_event(uuid, event);
	});
}

void handle_account_id(ClientContext& ctx, const AccountIDResponse* event) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;
	
	auto& auth_ctx = std::get<Context>(ctx.state_ctx);

	if(event->status != rpc::Account::Status::OK) {
		CLIENT_ERROR_FILTER_GLOB(LF_NETWORK, ctx)
			<< "Account server returned "
			<< util::fb_status(event->status, rpc::Account::EnumNamesStatus())
			<< " for " << auth_ctx.packet->username << " lookup" << LOG_ASYNC;

		auth_state(ctx, State::FAILED);
		ctx.handler->close();
		return;
	}

	if(event->account_id) {
		auth_ctx.account_id = event->account_id;
		fetch_session_key(ctx, event->account_id);
	} else {
		CLIENT_DEBUG_FILTER_GLOB(LF_NETWORK, ctx)
			<< "Account ID lookup for failed for "
			<< auth_ctx.packet->username << LOG_ASYNC;
		auth_state(ctx, State::FAILED);
		ctx.handler->close();
	}
}

void fetch_session_key(ClientContext& ctx, const std::uint32_t account_id) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	const auto& uuid = ctx.handler->uuid();

	Locator::account()->locate_session(account_id, [uuid](auto status, auto key) {
		SessionKeyResponse event(status, key);
		Locator::dispatcher()->post_event(uuid, event);
	});
}

void handle_session_key(ClientContext& ctx, const SessionKeyResponse* event) {
	const auto& auth_ctx = std::get<Context>(ctx.state_ctx);

	CLIENT_DEBUG_FILTER_GLOB(LF_NETWORK, ctx)
		<< "Account server returned "
		<< util::fb_status(event->status, rpc::Account::EnumNamesStatus())
		<< " for " << auth_ctx.packet->username << LOG_ASYNC;

	if(event->status == rpc::Account::Status::OK) {
		prove_session(ctx, event->key);
	} else {
		auth_state(ctx, State::FAILED);
		ctx.handler->close();
	}
}

void prove_session(ClientContext& ctx, const Botan::BigInt& key) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	// Encode the key without requiring an allocation
	static constexpr auto key_size_hint = 40u;

	boost::container::small_vector<std::uint8_t, key_size_hint> k_bytes(
		key.bytes(), boost::container::default_init
	);

	key.binary_encode(k_bytes.data(), k_bytes.size());

	const std::uint32_t protocol_id = 0; // best guess, this is hardcoded to zero in the client
	auto& auth_ctx = std::get<Context>(ctx.state_ctx);
	const auto& packet = auth_ctx.packet;

	auto hasher = Botan::HashFunction::create_or_throw("SHA-1");
	std::array<std::uint8_t, 20> hash;
	BOOST_ASSERT_MSG(hash.size() == hasher->output_length(), "Bad hash length");
	hasher->update(packet->username);
	hasher->update_be(protocol_id);
	hasher->update(packet->seed.data(), sizeof(packet->seed));
	hasher->update_be(boost::endian::native_to_big(auth_ctx.seed));
	hasher->update(k_bytes.data(), k_bytes.size());
	hasher->final(hash.data());

	if(hash != packet->digest) {
		CLIENT_DEBUG_GLOB(ctx) << "Received bad digest for " << packet->username << LOG_ASYNC;
		auth_state(ctx, State::FAILED);
		ctx.handler->close(); // key mismatch, client can't decrypt response
		return;
	}

	ctx.connection->set_key(k_bytes);
	ctx.client_id = { auth_ctx.account_id, packet->username };

	 // todo, allowing for multiple gateways to connect to a single world server
	 // will require an external service to keep track of available slots
	unsigned int active_players = 0;

	if(active_players < Locator::config()->max_slots) {
		auth_success(ctx);
	} else {
		auth_queue(ctx);
	}

	ctx.handler->stop_timer();
}

void send_auth_challenge(ClientContext& ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;
	auto& auth_ctx = std::get<Context>(ctx.state_ctx);
	protocol::SMSG_AUTH_CHALLENGE response;
	response->seed = auth_ctx.seed = gsl::narrow_cast<std::uint32_t>(rng::xorshift::next());
	ctx.connection->send(response);
}

void send_addon_data(ClientContext& ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	const auto& auth_ctx = std::get<Context>(ctx.state_ctx);
	const auto& addons = auth_ctx.packet->addons;
	protocol::SMSG_ADDON_INFO response;

	// todo, use AddonData.dbc
	for(const auto& addon : addons) {
		CLIENT_DEBUG_GLOB(ctx) << "Addon: " << addon.name << ", Key version: " << addon.key_version
			<< ", CRC: " << addon.crc << ", URL CRC: " << addon.update_url_crc << LOG_ASYNC;

		protocol::server::AddonInfo::AddonData data;
		data.type = protocol::server::AddonInfo::AddonData::Type::BLIZZARD;
		data.update_available_flag = 0; // URL must be present for this to work (check URL CRC)

		if(addon.key_version != 0 && addon.crc != 0x4C1C776D) { // todo, define?
			CLIENT_DEBUG_GLOB(ctx) << "Repairing " << addon.name << LOG_ASYNC;
			data.key_version = 1;
		} else {
			data.key_version = 0;
		}		
		
		response->addon_data.emplace_back(std::move(data));
	}

	ctx.connection->send(response);
}

void auth_queue(ClientContext& ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	const auto& uuid = ctx.handler->uuid();

	Locator::queue()->enqueue(uuid,
		[uuid](const std::size_t position) {
			Locator::dispatcher()->post_event(uuid, QueuePosition(position));
		},
		[uuid]() {
			const Event event { EventType::QUEUE_SUCCESS };
			Locator::dispatcher()->post_event(uuid, event);
		}
	);

	auth_state(ctx, State::IN_QUEUE);
	CLIENT_DEBUG_GLOB(ctx) << "added to queue" << LOG_ASYNC;
}

void auth_success(ClientContext& ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	send_auth_result(ctx, protocol::Result::AUTH_OK);
	send_addon_data(ctx);
	auth_state(ctx, State::SUCCESS);
	ctx.handler->state_update(ClientState::CHARACTER_LIST);
	CLIENT_DEBUG_GLOB(ctx) << "authenticated" << LOG_ASYNC;
}

void send_auth_result(ClientContext& ctx, protocol::Result result) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	protocol::SMSG_AUTH_RESPONSE response;
	response->result = result;
	ctx.connection->send(response);
}

void handle_queue_update(ClientContext& ctx, const QueuePosition* event) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;

	protocol::SMSG_AUTH_RESPONSE packet;
	packet->result = protocol::Result::AUTH_WAIT_QUEUE;
	packet->queue_position = gsl::narrow_cast<std::uint32_t>(event->position);
	ctx.connection->send(packet);
}

void handle_queue_success(ClientContext& ctx) {
	LOG_TRACE_FILTER_GLOB(LF_NETWORK) << log_func << LOG_ASYNC;
	auth_success(ctx);
}

void handle_timeout(ClientContext& ctx) {
	CLIENT_DEBUG_GLOB(ctx) << "Authentication timed out" << LOG_ASYNC;
	ctx.handler->close();
}

void enter(ClientContext& ctx) {
	ctx.state_ctx = Context{};
	ctx.handler->start_timer(AUTH_TIMEOUT);
	send_auth_challenge(ctx);
}

void handle_packet(ClientContext& ctx, protocol::ClientOpcode opcode) {
	switch(opcode) {
		case protocol::ClientOpcode::CMSG_AUTH_SESSION:
			handle_authentication(ctx);
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
		case EventType::ACCOUNT_ID_RESPONSE:
			handle_account_id(ctx, static_cast<const AccountIDResponse*>(event));
			break;
		case EventType::SESSION_KEY_RESPONSE:
			handle_session_key(ctx, static_cast<const SessionKeyResponse*>(event));
			break;
		case EventType::QUEUE_UPDATE_POSITION:
			handle_queue_update(ctx, static_cast<const QueuePosition*>(event));
			break;
		case EventType::QUEUE_SUCCESS:
			handle_queue_success(ctx);
			break;
		default:
			break;
	}
}

void exit(ClientContext& ctx) {
	const auto& auth_ctx = std::get<Context>(ctx.state_ctx);

	if(auth_ctx.state == State::IN_QUEUE) {
		Locator::queue()->dequeue(ctx.handler->uuid());
	}
}

} // authentication, gateway, ember
