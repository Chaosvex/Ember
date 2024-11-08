/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "SessionManager.h"
#include "SocketType.h"
#include <logger/LoggerFwd.h>
#include <shared/ClientRef.h>
#include <shared/memory/ASIOAllocator.h>
#include <shared/threading/ServicePool.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <string>
#include <cstddef>

namespace ember {

namespace bai = boost::asio::ip;

class NetworkListener final {
	using tcp_acceptor = boost::asio::basic_socket_acceptor<
		boost::asio::ip::tcp, boost::asio::io_context::executor_type>;

	SessionManager sessions_;
	tcp_acceptor acceptor_;
	ServicePool& pool_;
	std::size_t index_;
	tcp_socket socket_;
	log::Logger& logger_;

	void accept_connection();

public:
	NetworkListener(ServicePool& pool, const std::string& interface, std::uint16_t port,
	                bool tcp_no_delay, log::Logger& logger)
	                : acceptor_(
	                      pool.get(), 
	                      bai::tcp::endpoint(bai::address::from_string(interface), port)
	                  ),
	                  pool_(pool),
	                  index_(0),
	                  socket_(pool.get(0)),
	                  logger_(logger) {
		acceptor_.set_option(bai::tcp::no_delay(tcp_no_delay));
		acceptor_.set_option(bai::tcp::acceptor::reuse_address(true));
		accept_connection();
	}

	std::uint16_t port() const;
	void shutdown();
};

} // ember