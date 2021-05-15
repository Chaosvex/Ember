/*
 * Copyright (c) 2021 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Handler.h"
#include "DNSDefines.h"
#include "Socket.h"
#include <boost/asio.hpp>
#include <span>
#include <string_view>
#include <cstddef>

namespace ember::dns {

class MulticastSocket : public Socket {
    boost::asio::io_context& context_;
    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint ep_, remote_ep_;

    Handler* handler_;
    std::array<std::byte, MAX_DGRAM_LEN> buffer_;

    void receive();
    void handle_datagram(const std::span<std::byte> datagram, const boost::asio::ip::udp::endpoint ep);

public:
    MulticastSocket(boost::asio::io_context& context,
                    const std::string& listen_addr,
                    const std::string& mcast_group,
                    std::uint16_t port);

    void send() override;
    void register_handler(Handler* handler) override;
};

} // dns, ember