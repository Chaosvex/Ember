/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "NetworkListener.h"
#include <logger/Logger.h>
#include "FilterTypes.h"
#include "ClientConnection.h"
#include <memory>
#include <utility>

namespace ember {

void NetworkListener::accept_connection() {
	LOG_TRACE_FILTER(logger_, LF_NETWORK) << log_func << LOG_ASYNC;

	if(!acceptor_.is_open()) {
		return;
	}

	acceptor_.async_accept(socket_, [this](boost::system::error_code ec) {
		if(ec == boost::asio::error::operation_aborted) {
			return;
		}

		if(!ec) {
			const auto ep = socket_.remote_endpoint(ec);

			if(!ec) {
				LOG_DEBUG_FILTER(logger_, LF_NETWORK)
					<< "Accepted connection " << ep.address().to_string() << LOG_ASYNC;

				auto client = std::make_unique<ClientConnection>(
					sessions_, std::move(socket_), ClientRef(index_), logger_
				);

				sessions_.start(std::move(client));
			} else {
				LOG_DEBUG_FILTER(logger_, LF_NETWORK)
					<< "Aborted connection, remote peer disconnected" << LOG_ASYNC;
			}
		}

		++index_;
		index_ %= pool_.size();
		socket_ = tcp_socket(pool_.get(index_));
		accept_connection();
	});
}

void NetworkListener::shutdown() {
	LOG_TRACE_FILTER(logger_, LF_NETWORK) << log_func << LOG_ASYNC;
	acceptor_.close();
	sessions_.stop_all();
}

std::uint16_t NetworkListener::port() const {
	return acceptor_.local_endpoint().port();
}

} // ember