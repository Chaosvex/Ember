/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <dbcreader/Storage.h>
#include <cstdint>

namespace ember {

class Weather final {
	const dbc::Storage& data_;

public:
	Weather(std::uint32_t map, const dbc::Storage& data);
};

} // ember