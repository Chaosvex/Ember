/*
 * Copyright (c) 2021 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "MulticastSocket.h"
#include <logger/Logger.h>
#include <boost/asio/ip/multicast.hpp>

namespace ember::dns {

MulticastSocket::MulticastSocket(boost::asio::io_context& context,
                                 const std::string& listen_iface,
                                 const std::string& mcast_group,
                                 const std::uint16_t port)
                                 : context_(context),
                                   socket_(context),
                                   ep_(boost::asio::ip::address::from_string(mcast_group), port) {
    const auto mcast_iface = boost::asio::ip::address::from_string(listen_iface);
    const auto group_ip = boost::asio::ip::address::from_string(mcast_group);

	boost::asio::ip::udp::endpoint ep(mcast_iface, port);
	socket_.open(ep.protocol());
	socket_.set_option(boost::asio::ip::udp::socket::reuse_address(true));
	boost::asio::ip::multicast::join_group join_opt{};

	// ASIO is doing something weird on Windows, this is a hack
	if(mcast_iface.is_v4()) {
		join_opt = boost::asio::ip::multicast::join_group(group_ip.to_v4(), mcast_iface.to_v4());
	} else {
		join_opt = boost::asio::ip::multicast::join_group(group_ip);
	}

	socket_.set_option(join_opt);
	socket_.bind(ep);
    receive();
}

void MulticastSocket::receive() {
	LOG_TRACE_GLOB << log_func << LOG_ASYNC;

	if(!socket_.is_open()) {
		return;
	}

	socket_.async_receive(boost::asio::buffer(buffer_.data(), buffer_.size()),
        [this](const boost::system::error_code& ec, const std::size_t size) {
            if(ec == boost::asio::error::operation_aborted) {
		        return;
	        }

	        if(!ec) {
		        handle_datagram({buffer_.data(), size}, remote_ep_);
	        }

	        receive();
        }
    );
}

void MulticastSocket::handle_datagram(const std::span<const std::uint8_t> datagram,
                                      const boost::asio::ip::udp::endpoint& ep) {
	LOG_TRACE_GLOB << log_func << LOG_ASYNC;

    if(!handler_) {
		LOG_ERROR_GLOB << "No packet handler installed?" << LOG_ASYNC; // todo, rework
		return;
    }

	auto max_size = 0u;

	if(ep.protocol() == boost::asio::ip::udp::v4()) {
		max_size = MAX_DGRAM_PAYLOAD_IPV4;
	} else if(ep.protocol() == boost::asio::ip::udp::v6()) {
		max_size = MAX_DGRAM_PAYLOAD_IPV6;
	} else {
		LOG_ERROR_GLOB
			<< "Apparently this isn't IPv4 or IPv6, so congratulations on the"
			<< " interplanetary IoT network or bug in the library."
			<< LOG_ASYNC;
		max_size = MAX_DGRAM_PAYLOAD_IPV6;
	}

	if(datagram.size() > max_size) {
		LOG_WARN_GLOB
			<< "Datagram exceeded maximum permitted size of "
			<< max_size << " bytes. Skipping." << LOG_ASYNC;
		return;
	}

    handler_->handle_datagram(datagram);
}

void MulticastSocket::send(std::unique_ptr<std::vector<std::uint8_t>> buffer) {
	LOG_TRACE_GLOB << log_func << LOG_ASYNC;

	if(!socket_.is_open()) {
		return;
	}

	const auto ba_buf = boost::asio::buffer(*buffer);

	socket_.async_send_to(ba_buf, ep_,
		[buff = std::move(buffer)](const boost::system::error_code& ec, std::size_t size) {
			if(ec) {
				LOG_ERROR_GLOB
					<< "Error on sending datagram: "
					<< ec.message() << ", size " << size << LOG_ASYNC;
			}
		}
	);
}

void MulticastSocket::register_handler(Handler* handler) {
	LOG_TRACE_GLOB << log_func << LOG_ASYNC;
    handler_ = handler;
}

void MulticastSocket::deregister_handler(const Handler* handler) {
	LOG_TRACE_GLOB << log_func << LOG_ASYNC;

	if(handler != handler_) {
		LOG_ERROR_GLOB << "Attempted to deregister handler that wasn't registered" << LOG_ASYNC;
	}

	handler_ = nullptr;
}

void MulticastSocket::close() {
	boost::system::error_code ec; // we don't care about any errors
	socket_.shutdown(boost::asio::ip::udp::socket::shutdown_both, ec);
	socket_.close(ec);
}

} // dns, ember