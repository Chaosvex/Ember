/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <stun/Logging.h>
#include <stun/Protocol.h>
#include <boost/asio/steady_timer.hpp>
#include <array>
#include <chrono>
#include <expected>
#include <future>
#include <memory>
#include <variant>
#include <vector>
#include <cstddef>
#include <cstdint>

namespace ember::stun {

using MappedResult     = std::expected<attributes::MappedAddress, ErrorRet>;
using AttributesResult = std::expected<std::vector<attributes::Attribute>, ErrorRet>;
using NATResult        = std::expected<bool, ErrorRet>;
using HairpinResult    = std::expected<Hairpinning, ErrorRet>;
using BehaviourResult  = std::expected<Behaviour, ErrorRet>;

namespace detail {

enum class State {
	BASIC,
	MAPPING_TEST_1,
	MAPPING_TEST_2,
	MAPPING_TEST_3,
	FILTERING_TEST_1,
	FILTERING_TEST_2,
	FILTERING_TEST_3,
	HAIRPIN,
	HAIRPIN_AWAIT_RESP
};

struct Transaction {
	Transaction(boost::asio::any_io_executor ctx, std::chrono::milliseconds timeout, int max_retries)
		: timer(ctx), timeout(timeout), initial_to(timeout),
		retries_left(max_retries), max_retries(max_retries) {}

	// :grimacing:
	using Promise = std::variant<
		std::promise<MappedResult>,
		std::promise<AttributesResult>,
		std::promise<NATResult>,
		std::promise<BehaviourResult>,
		std::promise<HairpinResult>
	>;

	TxID id{};
	std::size_t key{};
	std::chrono::milliseconds timeout;
	const std::chrono::milliseconds initial_to;
	Promise promise;
	boost::asio::steady_timer timer;
	const int max_retries{};
	int retries_left{};
	int redirects{};
	std::shared_ptr<std::vector<std::uint8_t>> retry_buffer;
	std::vector<attributes::Attribute> attributes;
	State state{};

	struct TestData {
		attributes::XorMappedAddress xmapped{};
		attributes::OtherAddress otheradd{};
		Behaviour behaviour_result{};
		Hairpinning hairpinning_result{};
	} data;
};

}} // detail, stun, ember