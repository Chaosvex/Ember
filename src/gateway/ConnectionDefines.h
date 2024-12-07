/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "SocketType.h"
#include <spark/buffers/BinaryStream.h>
#include <spark/buffers/DynamicTLSBuffer.h>
#include <spark/buffers/StaticBuffer.h>

namespace ember::gateway {

static constexpr auto INBOUND_SIZE  { 1024 };
static constexpr auto OUTBOUND_SIZE { 2048 };

#if defined TARGET_PLAYER_COUNT && defined TARGET_WORKER_COUNT
static constexpr std::size_t PREALLOC_NODES {  TARGET_PLAYER_COUNT / TARGET_WORKER_COUNT };
#else
static constexpr std::size_t PREALLOC_NODES {  16 };
#endif

using StaticBuffer  = spark::io::StaticBuffer<std::uint8_t, INBOUND_SIZE>;
using DynamicBuffer = spark::io::DynamicTLSBuffer<OUTBOUND_SIZE, PREALLOC_NODES>;

using BinaryStream = spark::io::BinaryStream<StaticBuffer>;

} // gateway, ember