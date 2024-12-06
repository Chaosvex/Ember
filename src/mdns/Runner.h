/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <logger/LoggerFwd.h>
#include <shared/util/cstring_view.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>

namespace ember::dns {

static constexpr cstring_view APP_NAME { "MDNS-SD" };

int run(const boost::program_options::variables_map& args, log::Logger& logger);
void stop();

boost::program_options::options_description options();

} // dns, ember