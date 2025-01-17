/*
 * Copyright (c) 2016 - 2018 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Packet.h"
#include "Opcodes.h"
#include <protocol/server/AuthChallenge.h>
#include <protocol/server/AuthResponse.h>
#include <protocol/server/Pong.h>
#include <protocol/server/CharacterEnum.h>
#include <protocol/server/CharacterCreate.h>
#include <protocol/server/CharacterDelete.h>
#include <protocol/server/CharacterRename.h>
#include <protocol/server/CharacterLoginFailed.h>
#include <protocol/server/LogoutComplete.h>
#include <protocol/server/AddonInfo.h>
#include <protocol/client/AuthSession.h>
#include <protocol/client/Ping.h>
#include <protocol/client/CharacterCreate.h>
#include <protocol/client/CharacterDelete.h>
#include <protocol/client/CharacterEnum.h>
#include <protocol/client/CharacterRename.h>
#include <protocol/client/PlayerLogin.h>

namespace ember::protocol {

using SMSG_ADDON_INFO             = ServerPacket<ServerOpcode::SMSG_ADDON_INFO, server::AddonInfo>;
using SMSG_AUTH_CHALLENGE         = ServerPacket<ServerOpcode::SMSG_AUTH_CHALLENGE, server::AuthChallenge>;
using SMSG_AUTH_RESPONSE          = ServerPacket<ServerOpcode::SMSG_AUTH_RESPONSE, server::AuthResponse>;
using SMSG_PONG                   = ServerPacket<ServerOpcode::SMSG_PONG, server::Pong>;
using SMSG_CHAR_ENUM              = ServerPacket<ServerOpcode::SMSG_CHAR_ENUM, server::CharacterEnum>;
using SMSG_CHAR_CREATE            = ServerPacket<ServerOpcode::SMSG_CHAR_CREATE, server::CharacterCreate>;
using SMSG_CHAR_DELETE            = ServerPacket<ServerOpcode::SMSG_CHAR_DELETE, server::CharacterDelete>;
using SMSG_CHAR_RENAME            = ServerPacket<ServerOpcode::SMSG_CHAR_RENAME, server::CharacterRename>;
using SMSG_CHARACTER_LOGIN_FAILED = ServerPacket<ServerOpcode::SMSG_CHARACTER_LOGIN_FAILED, server::CharacterLoginFailed>;
using SMSG_LOGOUT_COMPLETE        = ServerPacket<ServerOpcode::SMSG_LOGOUT_COMPLETE, server::LogoutComplete>;

using CMSG_AUTH_SESSION           = ClientPacket<ClientOpcode::CMSG_AUTH_SESSION, client::AuthSession>;
using CMSG_PING                   = ClientPacket<ClientOpcode::CMSG_PING, client::Ping>;
using CMSG_CHAR_CREATE            = ClientPacket<ClientOpcode::CMSG_CHAR_CREATE, client::CharacterCreate>;
using CMSG_CHAR_DELETE            = ClientPacket<ClientOpcode::CMSG_CHAR_DELETE, client::CharacterDelete>;
using CMSG_CHAR_ENUM              = ClientPacket<ClientOpcode::CMSG_CHAR_ENUM, client::CharacterEnum>;
using CMSG_CHAR_RENAME            = ClientPacket<ClientOpcode::CMSG_CHAR_RENAME, client::CharacterRename>;
using CMSG_PLAYER_LOGIN           = ClientPacket<ClientOpcode::CMSG_PLAYER_LOGIN, client::PlayerLogin>;

} // protocol, ember

