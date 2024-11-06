/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <dbcreader/Storage.h>
#include <logger/LoggerFwd.h>
#include <span>
#include <string>
#include <cstdint>

namespace ember {

std::string random_tip(const dbc::Store<dbc::GameTips>& tips);
bool validate_maps(std::span<const std::int32_t> maps, const dbc::Store<dbc::Map>& dbc, log::Logger& logger);
void print_maps(std::span<const std::int32_t> maps, const dbc::Store<dbc::Map>& dbc, log::Logger& logger);

} // ember