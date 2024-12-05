/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <logger/Logger.h>
#include <boost/asio/io_context.hpp>
#include <boost/program_options.hpp>
#include <shared/util/cstring_view.hpp>
#include <semaphore>
#include <span>

namespace ember::dns {

static constexpr ember::cstring_view APP_NAME { "MDNS-SD" };

int run(std::span<const char*> cmd_args);
void stop();

} // dns, ember