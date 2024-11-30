/*
 * Copyright (c) 2021 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Spark_generated.h"
#include <spark/Connection.h>
#include <spark/Channel.h>
#include <spark/Handler.h>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <logger/Logger.h>
#include <gsl/pointers>
#include <array>
#include <chrono>
#include <concepts>
#include <memory>
#include <string>
#include <utility>

namespace ember::spark {

class HandlerRegistry;
using namespace std::chrono_literals;

static auto LATENCY_WARN_THRESHOLD = 100ms; // todo, move?

class RemotePeer final {
	enum class State {
		HELLO, NEGOTIATING, DISPATCHING
	} state_ = State::HELLO;

	boost::asio::io_context& ctx_;
	std::shared_ptr<Connection> conn_;
	std::string banner_;
	std::string remote_banner_;
	HandlerRegistry& registry_;
	std::array<std::shared_ptr<Channel>, 256> channels_{};
	log::Logger& log_;

	std::uint32_t ping_sequence_ = 0;
	std::chrono::steady_clock::time_point ping_time_;

	void send(Message&& msg);
	Handler* find_handler(const core::OpenChannel& msg);
	std::uint8_t next_empty_channel();

	void handle_control_message(std::span<const std::uint8_t> data);
	void handle_channel_message(const MessageHeader& header, std::span<const std::uint8_t> data);
	void handle_open_channel(const core::OpenChannel* msg);
	void handle_open_channel_response(const core::OpenChannelResponse* msg);
	void handle_close_channel(const core::CloseChannel* msg);
	void handle_ping(const core::Ping* msg);
	void handle_pong(const core::Pong* msg);

	void open_channel_response(core::Result result, std::uint8_t id, std::uint8_t requested);
	void send_close_channel(std::uint8_t id);
	void send_open_channel(std::string name, std::string type, std::uint8_t id);
	void receive(std::span<const std::uint8_t> data);

public:
	RemotePeer(boost::asio::io_context& ctx, Connection connection,
	           std::string banner, std::string remote_banner,
	           HandlerRegistry& registry, log::Logger& log);
	~RemotePeer();

	RemotePeer(RemotePeer&& peer) = delete;
	RemotePeer operator=(RemotePeer&&) = delete;
	RemotePeer(RemotePeer&) = delete;
	RemotePeer operator=(RemotePeer&) = delete;

	void open_channel(std::string type, gsl::not_null<Handler*> handler);
	void remove_handler(gsl::not_null<Handler*> handler);
	void start();
};

} // spark, ember