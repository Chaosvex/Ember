/*
 * Copyright (c) 2016 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

namespace ember::gateway {

struct ServerConfig {
	unsigned int compression_level;
	unsigned int max_bandwidth_in;
	unsigned int max_bandwidth_out;
};

} // gateway, ember