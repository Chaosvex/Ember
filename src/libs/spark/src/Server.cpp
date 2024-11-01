/*
 * Copyright (c) 2021 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <spark/Server.h>
#include <spark/Handler.h>
#include <spark/Connection.h>
#include <spark/buffers/BufferAdaptor.h>
#include <spark/buffers/BinaryStream.h>
#include <spark/Utility.h>
#include <logger/Logger.h>
#include <shared/FilterTypes.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <format>
#include <memory>

namespace ember::spark {

namespace ba = boost::asio;

Server::Server(boost::asio::io_context& context, std::string_view name,
               const std::string& iface, const std::uint16_t port, log::Logger& logger)
	: ctx_(context),
	  acceptor_(context, ba::ip::tcp::endpoint(ba::ip::address::from_string(iface), port)),
	  resolver_(context),
	  logger_(logger),
	  stopped_(false) {
	acceptor_.set_option(ba::ip::tcp::no_delay(true));
	acceptor_.set_option(ba::ip::tcp::acceptor::reuse_address(true));

	// generate random unique name for this service
	const auto uuid = boost::uuids::random_generator()();
	name_ = std::format("{}:{}", name, boost::uuids::to_string(uuid));
	ba::co_spawn(ctx_, listen(), ba::detached);
}

ba::awaitable<void> Server::listen() {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	while(acceptor_.is_open()) {
		co_await accept_connection();
	}
}

ba::awaitable<void> Server::accept_connection() {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	ba::ip::tcp::socket socket(ctx_);
	auto [ec] = co_await acceptor_.async_accept(socket, as_tuple(ba::deferred));

	if(ec == boost::asio::error::operation_aborted) {
		co_return;
	}

	if(ec) {
		LOG_DEBUG(logger_)
			<< "[spark] Unable to accept connection"
			<< LOG_ASYNC;
		co_return;
	}

	const auto ep = socket.remote_endpoint(ec);

	if(ec) {
		LOG_DEBUG(logger_)
			<< "[spark] Unable to obtain endpoint, remote peer disconnected"
			<< LOG_ASYNC;
		co_return;
	}

	LOG_DEBUG_FILTER(logger_, LF_SPARK)
		<< "[spark] Accepted connection "
		<< ep.address().to_string()
		<< ":" << ep.port()
		<< LOG_ASYNC;

	co_await accept(std::move(socket));
}

ba::awaitable<void> Server::accept(boost::asio::ip::tcp::socket socket) try {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	auto ep = socket.remote_endpoint();
	const auto key = std::format("{}:{}", ep.address().to_string(), std::to_string(ep.port()));

	Connection connection(std::move(socket), logger_, [this, key]() {
		close_peer(key);
	});

	auto banner = co_await receive_banner(connection);
	co_await send_banner(connection, name_);

	LOG_INFO_FILTER(logger_, LF_SPARK)
		<< std::format("[spark] Connected to {}", banner)
		<< LOG_ASYNC;

	auto peer = std::make_shared<RemotePeer>(
		ctx_, std::move(connection), name_, banner, handlers_, logger_
	);

	peer->start();
	peers_.add(key, std::move(peer));
} catch(const std::exception& e) {
	LOG_WARN_FILTER(logger_, LF_SPARK) << e.what() << LOG_ASYNC;
}

void Server::register_handler(gsl::not_null<Handler*> handler) {
	handlers_.register_service(handler);

	LOG_TRACE_FILTER(logger_, LF_SPARK)
		<< "[spark] Registered handler for "
		<< handler->type()
		<< LOG_ASYNC;
}

void Server::deregister_handler(gsl::not_null<Handler*> handler) {
	handlers_.deregister_service(handler);
	peers_.notify_remove_handler(handler);

	LOG_TRACE_FILTER(logger_, LF_SPARK)
		<< "[spark] Removed handler for "
		<< handler->type()
		<< LOG_ASYNC;
}

ba::awaitable<std::shared_ptr<RemotePeer>>
Server::connect(std::string_view host, const std::uint16_t port) try {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	auto results = co_await resolver_.async_resolve(
		host, std::to_string(port), ba::deferred
	);

	ba::ip::tcp::socket socket(ctx_);
	co_await ba::async_connect(socket, results.begin(), results.end(), ba::deferred);

	const auto key = std::format("{}:{}", host, port);

	Connection connection(std::move(socket), logger_, [this, key]() {
		close_peer(key);
	});

	co_await send_banner(connection, name_);
	auto banner = co_await receive_banner(connection);

	LOG_INFO_FILTER(logger_, LF_SPARK)
		<< std::format("[spark] Connected to {}", banner)
		<< LOG_ASYNC;

	auto peer = std::make_shared<RemotePeer>(
		ctx_, std::move(connection), name_, banner, handlers_, logger_
	);

	co_return peer;
} catch(const std::exception& e) {
	const auto msg = std::format(
		"[spark] Could not connect to {}:{} ({})", host, port, e.what()
	);

	LOG_DEBUG_FILTER(logger_, LF_SPARK) << msg << LOG_ASYNC;
	co_return nullptr;
}

ba::awaitable<void> Server::try_open(std::string host, std::uint16_t port,
                                     std::string service, gsl::not_null<Handler*> handler) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	const auto key = std::format("{}:{}", host, port);

	// check if we've already established a connection with this peer
	if(const auto& peer = peers_.find(key); peer) {
		peer->open_channel(std::move(service), handler);
		co_return;
	}

	// open a connection if we didn't find one
	auto peer = co_await connect(host, port);

	if(!peer) {
		handler->connect_failed(host, port);
		co_return;
	}

	peer->start();
	peer->open_channel(std::move(service), handler);
	peers_.add(key, std::move(peer));
}

void Server::connect(std::string_view host, const std::uint16_t port,
                     std::string_view service, gsl::not_null<Handler*> handler) {
	connect(std::string(host), port, std::string(service), handler);
}

void Server::connect(std::string host, const std::uint16_t port,
                     std::string service, gsl::not_null<Handler*> handler) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	ba::co_spawn(ctx_, try_open(
		std::move(host), port, std::move(service), handler), ba::detached
	);
}

ba::awaitable<void> Server::send_banner(Connection& conn, const std::string& banner) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	core::HelloT hello {
		.description = banner
	};

	Message msg;
	finish(hello, msg);
	write_header(msg);
	co_await conn.send(msg);
}


ba::awaitable<std::string> Server::receive_banner(Connection& conn) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	auto msg = co_await conn.receive_msg();
	spark::io::BufferAdaptor adaptor(msg);
	spark::io::BinaryStream stream(adaptor);

	MessageHeader header;

	if(header.read_from_stream(stream) != MessageHeader::State::OK
	   || header.size <= stream.total_read()) {
		throw exception("bad message header");
	}

	const auto header_size = stream.total_read();
	std::span flatbuffer(msg.data() + header_size, msg.size_bytes() - header_size);

	flatbuffers::Verifier verifier(flatbuffer.data(), flatbuffer.size());
	auto fb = core::GetHeader(flatbuffer.data());
	auto hello = fb->message_as_Hello();

	if(!hello || !hello->Verify(verifier)) {
		throw exception("bad flatbuffer message");
	}

	co_return hello->description()->str();
}

// todo, need to link down to all channels, error code
void Server::close_peer(const std::string& key) {
	//LOG_TRACE(logger_) << log_func << LOG_ASYNC;
	//peers_.remove(key);

	//LOG_DEBUG_FILTER(logger_, LF_SPARK)
	//	<< "[spark] Close connection " << key << LOG_ASYNC;
}

void Server::shutdown() {
	if(stopped_) {
		return;
	}

	LOG_DEBUG_FILTER(logger_, LF_SPARK)
		<< "[spark] Service shutting down..."
		<< LOG_ASYNC;

	acceptor_.close();
	stopped_ = true;
}

Server::~Server() {
	shutdown();
}

std::uint16_t Server::port() const {
	return acceptor_.local_endpoint().port();
}

} // spark, ember