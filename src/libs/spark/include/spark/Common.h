/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <spark/Result.h>
#include <flatbuffers/flatbuffer_builder.h>
#include <boost/container/small_vector.hpp>
#include <boost/uuid/uuid.hpp>
#include <expected>
#include <functional>
#include <span>
#include <cstdint>

namespace ember::spark {

using Token = boost::uuids::uuid;

struct Link;
struct Message {
	boost::container::small_vector<std::uint8_t, 1000> header; // temp
	flatbuffers::FlatBufferBuilder fbb;
};

using MessageResult = std::expected<std::span<const std::uint8_t>, Result>;

using TrackedState = std::function<void(
	const spark::Link& link, MessageResult
)>;

} // spark, ember