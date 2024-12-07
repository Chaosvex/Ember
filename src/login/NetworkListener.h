/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "FilterTypes.h"
#include "NetworkSession.h"
#include "SessionBuilders.h"
#include "SessionManager.h"
#include "SocketType.h"
#include <logger/Logger.h>
#include <shared/IPBanCache.h>
#include <shared/memory/ASIOAllocator.h>
#include <shared/metrics/Metrics.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <string>
#include <utility>
#include <cstdint>
#include <cstddef>

namespace ember {

class NetworkListener final {
	using tcp_acceptor = boost::asio::basic_socket_acceptor<
		boost::asio::ip::tcp, boost::asio::io_context::executor_type>;

	boost::asio::io_context& io_context_;
	tcp_acceptor acceptor_;
	tcp_socket socket_;

	SessionManager sessions_;
	const NetworkSessionBuilder& session_builder_;
	log::Logger& logger_;
	Metrics& metrics_;
	IPBanCache& ban_list_;

	void accept_connection() {
		LOG_TRACE_FILTER(logger_, LF_NETWORK) << log_func << LOG_ASYNC;

		if(!acceptor_.is_open()) {
			return;
		}

		acceptor_.async_accept(socket_, [this](boost::system::error_code ec) {
			if(ec == boost::asio::error::operation_aborted) {
				return;
			}

			if(!ec) {
				const auto& ep = socket_.remote_endpoint(ec);

				if(ec) {
					LOG_DEBUG_FILTER(logger_, LF_NETWORK)
						<< "Aborted connection, remote peer disconnected" << LOG_ASYNC;
					metrics_.increment("aborted_connections");
					return;
				}

				const auto& ip = ep.address();

				if(!ban_list_.is_banned(ip)) {
					LOG_DEBUG_FILTER(logger_, LF_NETWORK)
						<< "Accepted connection " << ip.to_string() << LOG_ASYNC;
					metrics_.increment("accepted_connections");
					start_session(std::move(socket_));
				} else {
					LOG_DEBUG_FILTER(logger_, LF_NETWORK)
						<< "Rejected connection " << ip.to_string()
						<< " from banned IP range" << LOG_ASYNC;
					metrics_.increment("rejected_connections");
				}
			}

			socket_ = tcp_socket(boost::asio::make_strand(io_context_));
			accept_connection();
		});
	}

	void start_session(tcp_socket socket) {
		LOG_TRACE_FILTER(logger_, LF_NETWORK) << log_func << LOG_ASYNC;
		auto session = session_builder_.create(sessions_, std::move(socket), logger_);
		sessions_.start(session);
	}

public:
	NetworkListener(boost::asio::io_context& io_context, const std::string& interface, std::uint16_t port,
	                bool tcp_no_delay, const NetworkSessionBuilder& session_create, IPBanCache& bans,
	                log::Logger& logger, Metrics& metrics)
	                : acceptor_(io_context, boost::asio::ip::tcp::endpoint(
	                            boost::asio::ip::address::from_string(interface), port)),
	                  io_context_(io_context),
	                  socket_(boost::asio::make_strand(io_context)),
	                  session_builder_(session_create),
	                  logger_(logger),
	                  metrics_(metrics),
	                  ban_list_(bans) {
		acceptor_.set_option(boost::asio::ip::tcp::no_delay(tcp_no_delay));
		acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
		accept_connection();
	}

	void shutdown() {
		LOG_TRACE_FILTER(logger_, LF_NETWORK) << log_func << LOG_ASYNC;
		acceptor_.close();
		sessions_.stop_all();
	}

	std::size_t connection_count() const {
		return sessions_.count();
	}

	std::uint16_t port() const {
		return acceptor_.local_endpoint().port();
	}
};

} // ember