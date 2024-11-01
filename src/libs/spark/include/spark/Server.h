/*
 * Copyright (c) 2021 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <spark/Peers.h>
#include <spark/HandlerRegistry.h>
#include <spark/RemotePeer.h>
#include <logger/LoggerFwd.h>
#include <gsl/pointers>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/uuid/uuid.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace ember::spark {

class Connection;

class Server final {
	boost::asio::io_context& ctx_;
	boost::asio::ip::tcp::acceptor acceptor_;
	boost::asio::ip::tcp::resolver resolver_;
	Peers peers_;
	HandlerRegistry handlers_;
	std::string name_;
	log::Logger& logger_;
	bool stopped_;
	
	boost::asio::awaitable<void> listen();
	boost::asio::awaitable<void> accept_connection();
	boost::asio::awaitable<void> accept(boost::asio::ip::tcp::socket socket);
	boost::asio::awaitable<std::shared_ptr<RemotePeer>> connect(std::string_view host, std::uint16_t port);
	boost::asio::awaitable<void> send_banner(Connection& conn, const std::string& banner);
	boost::asio::awaitable<std::string> receive_banner(Connection& conn);
	boost::asio::awaitable<void> try_open(std::string host,
	                                      std::uint16_t port,
	                                      std::string service,
	                                      gsl::not_null<Handler*> handler);

	void close_peer(const std::string& key);

public:
	Server(boost::asio::io_context& context, std::string_view name,
	       const std::string& iface, std::uint16_t port, log::Logger& logger);
	~Server();

	void register_handler(gsl::not_null<Handler*> handler);
	void deregister_handler(gsl::not_null<Handler*> handler);

	std::uint16_t port() const;

	void connect(std::string_view host, std::uint16_t port,
	             std::string_view service, gsl::not_null<Handler*> handler);

	void connect(std::string host, std::uint16_t port,
	             std::string service, gsl::not_null<Handler*> handler);
	void shutdown();
};

} // spark, ember