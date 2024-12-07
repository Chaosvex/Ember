/*
 * Copyright (c) 2016 - 2018 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

namespace ember::gateway {

enum class EventType {
	QUEUE_SUCCESS,
    QUEUE_UPDATE_POSITION,
	ACCOUNT_ID_RESPONSE,
	SESSION_KEY_RESPONSE,
	CHAR_CREATE_RESPONSE,
	CHAR_DELETE_RESPONSE,
	CHAR_ENUM_RESPONSE,
	CHAR_RENAME_RESPONSE,
	PLAYER_LOGIN,
	TIMER_EXPIRED
};

} // gateway, ember