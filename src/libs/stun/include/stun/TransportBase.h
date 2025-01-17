/*
 * Copyright (c) 2023 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <boost/system/error_code.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <string_view>
#include <string>
#include <vector>
#include <cstdint>

namespace ember::stun {

class Transport {
public:
	using OnReceive = std::function<void(std::vector<std::uint8_t>)>;
	using OnConnectionError = std::function<void(const boost::system::error_code&)>;
	using OnConnect = std::function<void(const boost::system::error_code&)>;

	OnReceive rcb_;
	OnConnectionError ecb_;
	OnConnect ocb_;

	virtual void connect(std::string_view host, std::uint16_t port, OnConnect&& cb) = 0;
	virtual void close() = 0;
	virtual void send(std::vector<std::uint8_t> message) = 0;
	virtual void send(std::shared_ptr<std::vector<std::uint8_t>> message) = 0;
	virtual std::chrono::milliseconds timeout() const = 0;
	virtual unsigned int retries() const = 0;
	virtual std::string local_ip() const = 0;
	virtual std::uint16_t local_port() const = 0;

	virtual ~Transport() = default;

	virtual void set_callbacks(OnReceive rcb, OnConnectionError ecb) {
		if(!rcb || !ecb) {
			throw std::invalid_argument("Transport callbacks cannot be null");
		}

		rcb_ = rcb;
		ecb_ = ecb;
	}
};

} // stun, ember