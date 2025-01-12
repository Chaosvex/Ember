/*
* Copyright (c) 2020 - 2024 Ember
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include <logger/Logger.h>

#define CLIENT_TRACE(logger, ctx) \
	LOG_TRACE(logger) << ctx.handler->client_identify() << ": "

#define CLIENT_DEBUG(logger, ctx) \
	LOG_DEBUG(logger) << ctx.handler->client_identify() << ": "

#define CLIENT_INFO(logger, ctx) \
	LOG_INFO(logger) << ctx.handler->client_identify() << ": "

#define CLIENT_WARN(logger, ctx) \
    LOG_WARN(logger) << ctx.handler->client_identify() << ": "

#define CLIENT_ERROR(logger, ctx) \
	LOG_ERROR(logger) << ctx.handler->client_identify() << ": "

#define CLIENT_FATAL(logger, ctx) \
	LOG_FATAL(logger) << ctx.handler->client_identify() << ": "

#define CLIENT_TRACE_GLOB(ctx) \
	CLIENT_TRACE(ember::log::global_logger(), ctx)

#define CLIENT_DEBUG_GLOB(ctx) \
	CLIENT_DEBUG(ember::log::global_logger(), ctx)

#define CLIENT_INFO_GLOB(ctx) \
	CLIENT_INFO(ember::log::global_logger(), ctx)

#define CLIENT_WARN_GLOB(ctx) \
	CLIENT_WARN(ember::log::global_logger(), ctx)

#define CLIENT_ERROR_GLOB(ctx) \
	CLIENT_ERROR(ember::log::global_logger(), ctx)

#define CLIENT_FATAL_GLOB(ctx) \
	CLIENT_FATAL(ember::log::global_logger(), ctx)
