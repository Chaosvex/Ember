/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

namespace ember::spark {

enum class Result {
	OK,
	LINK_GONE,
	TIMED_OUT,
	CANCELLED,
	NET_ERROR,
	CHANNEL_CLOSED,
	WRONG_MESSAGE_TYPE
};

} // spark, ember