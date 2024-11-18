/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Weather.h"

namespace ember {

Weather::Weather(std::uint32_t map, const dbc::Storage& data)
	: data_(data) {
	
}

} // ember