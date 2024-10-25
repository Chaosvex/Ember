/*
 * Copyright (c) 2021 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <logger/Logger.h>
#include <spark/Common.h>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/container/small_vector.hpp>
#include <array>
#include <functional>
#include <memory>
#include <queue>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <cstddef>
#include <cstdint>

namespace ember::spark {

class Connection final {
public:
	using ReceiveHandler = std::function<void(std::span<const std::uint8_t>)>;
	using CloseHandler = std::function<void()>;

private:
	static constexpr auto INITIAL_BUFFER_SIZE = 100u;         // 8KB
	static constexpr auto MAXIMUM_BUFFER_SIZE = 1024u * 1024 ; // 1MB

	log::Logger& logger_;
	boost::asio::ip::tcp::socket socket_;
	boost::asio::strand<boost::asio::any_io_executor> strand_;
	boost::container::small_vector<std::uint8_t, INITIAL_BUFFER_SIZE> buffer_{};
	std::queue<Message> queue_;
	CloseHandler on_close_;

	void buffer_resize(const std::uint32_t size);
	boost::asio::awaitable<void> process_queue();
	boost::asio::awaitable<void> begin_receive(ReceiveHandler handler);
	boost::asio::awaitable<std::uint32_t> do_receive();
	boost::asio::awaitable<std::size_t> read_until(std::size_t offset, std::size_t read_size);

public:
	Connection(boost::asio::ip::tcp::socket socket, log::Logger& logger, CloseHandler handler);
	Connection(Connection&&) = default;

	std::string address() const;
	void send(Message&& buffer);
	boost::asio::awaitable<void> send(Message& msg);
	boost::asio::awaitable<std::span<std::uint8_t>> receive_msg();
	void start(ReceiveHandler handler);
	void close();
};

} // spark, ember