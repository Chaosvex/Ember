/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <spark/Connection.h>
#include <spark/Exception.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/endian/conversion.hpp>
#include <format>
#include <cassert>
#include <cstring>

namespace ba = boost::asio;

namespace ember::spark {

Connection::Connection(ba::ip::tcp::socket socket, log::Logger& logger, CloseHandler handler)
	: logger_(logger),
	  socket_(std::move(socket)),
      strand_(socket_.get_executor()),
	  on_close_(handler) {
	buffer_.resize(4);
}

ba::awaitable<void> Connection::process_queue() try {
	while(!queue_.empty()) {
		const auto msg = std::move(queue_.front());
		queue_.pop();

		std::array<ba::const_buffer, 2> buffers {
			ba::const_buffer { msg.header.data(), msg.header.size() },
			ba::const_buffer { msg.fbb.GetBufferPointer(), msg.fbb.GetSize() }
		};

		co_await ba::async_write(socket_, buffers, ba::deferred);
	}
} catch(std::exception& e) {
	close();
}

void Connection::send(Message&& buffer) {
	ba::post(strand_, [&, buffer = std::move(buffer)]() mutable {
		if(!socket_.is_open()) {
			return;
		}

		bool inactive = queue_.empty();
		queue_.emplace(std::move(buffer));

		if(inactive) {
			ba::co_spawn(strand_, process_queue(), ba::detached);
		}
	});
}

/*
 * This will read until at least the read_size has been received but
 * will read as much as possible into the buffer, with the hope that the
 * entire message will be received with one receive call
 */
ba::awaitable<std::size_t> Connection::read_until(const std::size_t offset,
                                                  const std::size_t read_size) {
	std::size_t received = offset;

	while(received < read_size) {
		auto buffer = ba::buffer(buffer_.data() + received, buffer_.size() - received);
		received += co_await socket_.async_receive(buffer, ba::deferred);
	}

	co_return received;
}

void Connection::buffer_resize(const std::uint32_t size) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	if(size > MAXIMUM_BUFFER_SIZE) {
		const auto log_msg = std::format(
			"maximum message ({}b) exceeded", MAXIMUM_BUFFER_SIZE
		);

		throw exception(log_msg);
	}

	LOG_TRACE_ASYNC(logger_, "Resizing RPC buffer to {}b", size);
	buffer_.resize(size);
}

ba::awaitable<std::uint32_t>  Connection::do_receive() {
	std::uint32_t msg_size = 0;

	// read the message size
	auto buf = boost::asio::buffer(buffer_.data(), sizeof(msg_size));
	co_await socket_.async_read_some(buf, ba::deferred);
	
	std::memcpy(&msg_size, buffer_.data(), sizeof(msg_size));
	boost::endian::little_to_native_inplace(msg_size);

	if(msg_size > buffer_.size()) {
		buffer_resize(msg_size);
	}

	buf = boost::asio::buffer(buffer_.data() + sizeof(msg_size), msg_size - sizeof(msg_size));
	co_await socket_.async_read_some(buf, ba::deferred);
	co_return msg_size;
}

ba::awaitable<void> Connection::begin_receive(ReceiveHandler handler) try {
	while(socket_.is_open()) {
		const auto msg_size = co_await do_receive();

		// message complete, handle it
		std::span view(buffer_.data(), msg_size);
		handler(view);
	}
} catch(std::exception& e) {
	LOG_WARN(logger_) << e.what() << LOG_ASYNC;
	close();
}

ba::awaitable<std::span<std::uint8_t>> Connection::receive_msg() {
	// read message size uint32
	std::uint32_t msg_size = 0;

	auto buffer = ba::buffer(buffer_.data(), sizeof(msg_size));
	co_await ba::async_read(socket_, buffer, ba::deferred);
	std::memcpy(&msg_size, buffer_.data(), sizeof(msg_size));

	if(msg_size > buffer_.size()) {
		buffer_resize(msg_size);
	}

	// read the rest of the message
	buffer = ba::buffer(buffer_.data() + sizeof(msg_size), msg_size - sizeof(msg_size));
	co_await ba::async_read(socket_, buffer, ba::deferred);
	co_return std::span{buffer_.data(), msg_size};
}

ba::awaitable<void> Connection::send(Message& msg) {
	std::array<ba::const_buffer, 2> buffers {
		ba::const_buffer { msg.header.data(), msg.header.size() },
		ba::const_buffer { msg.fbb.GetBufferPointer(), msg.fbb.GetSize() }
	};

	co_await ba::async_write(socket_, buffers, ba::deferred);
}

// start full-duplex send/receive
void Connection::start(ReceiveHandler handler) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;
	ba::co_spawn(strand_, begin_receive(handler), ba::detached);
}

void Connection::close() {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	socket_.close();

	if(on_close_) {
		on_close_();
	}
}

std::string Connection::address() const {
	if(!socket_.is_open()) {
		return "";
	}

	const auto& ep = socket_.remote_endpoint();
	return std::format("{}:{}", ep.address().to_string(), ep.port());
}

} // spark, ember