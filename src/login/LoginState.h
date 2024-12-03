/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

namespace ember {

enum class LoginState {
	CHALLENGE,
	PROOF,
	RECONNECT_PROOF,
	REQUEST_REALMS,

	SURVEY_INITIATE,
	SURVEY_TRANSFER,
	SURVEY_RESULT,

	PATCH_INITIATE,
	PATCH_TRANSFER,

	FETCHING_USER_LOGIN,
	FETCHING_USER_RECONNECT,
	FETCHING_SESSION,
	FETCHING_CHARACTER_DATA,

	WRITING_SESSION,
	WRITING_SURVEY,

	CLOSED
};

} // ember