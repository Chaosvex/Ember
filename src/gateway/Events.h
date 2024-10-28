/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Event.h"
#include "Account_generated.h"
#include "Character_generated.h"
#include <protocol/ResultCodes.h>
#include <protocol/Packets.h>
#include <shared/database/objects/Character.h>
#include <botan/bigint.h>
#include <vector>
#include <utility>
#include <cstdint>
#include <cstddef>

namespace ember {

struct PlayerLogin : Event {
	explicit PlayerLogin(std::uint64_t character_id)
		: Event{ EventType::PLAYER_LOGIN },
		  character_id_(character_id) { }

	const std::uint64_t character_id_;
};

struct QueuePosition : Event {
	explicit QueuePosition(std::size_t position)	
		: Event { EventType::QUEUE_UPDATE_POSITION },
	      position(position) { }

	std::size_t position;
};

struct AccountIDResponse : Event {
	AccountIDResponse(rpc::Account::Status status, std::uint32_t id)
		: Event { EventType::ACCOUNT_ID_RESPONSE },
	      status(status),
		  account_id(id) { }

	rpc::Account::Status status;
	std::uint32_t account_id;
};

struct SessionKeyResponse : Event {
	SessionKeyResponse(rpc::Account::Status status, Botan::BigInt key)
		: Event { EventType::SESSION_KEY_RESPONSE },
	      status(status),
		  key(std::move(key)) { }

	rpc::Account::Status status;
	Botan::BigInt key;
};

struct CharEnumResponse : Event {
	CharEnumResponse(rpc::Character::Status status, std::vector<Character> characters)
		: Event{ EventType::CHAR_ENUM_RESPONSE },
		  status(status),
		  characters(std::move(characters)) { }

	rpc::Character::Status status;
	std::vector<Character> characters;
};

struct CharCreateResponse : Event {
	explicit CharCreateResponse(protocol::Result result)
		: Event { EventType::CHAR_CREATE_RESPONSE },
		  result(result) { }

	protocol::Result result;
};

struct CharDeleteResponse : Event {
	explicit CharDeleteResponse(protocol::Result result)
		: Event{ EventType::CHAR_DELETE_RESPONSE },
		  result(result) { }

	protocol::Result result;
};

struct CharRenameResponse : Event {
	CharRenameResponse(protocol::Result result, std::uint64_t id, std::string name)
		: Event { EventType::CHAR_RENAME_RESPONSE },
		  result(result),
		  character_id(id),
		  name(std::move(name)) { }

	protocol::Result result;
	std::uint64_t character_id;
	std::string name;
};

} // ember