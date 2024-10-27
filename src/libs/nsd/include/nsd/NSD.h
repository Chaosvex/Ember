/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <DiscoveryClientStub.h>
#include <logger/Logger.h>
#include <spark/Server.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <cstdint>

namespace ember {

class NetworkServiceDiscovery final : public services::DiscoveryClient {
	inline static const std::string_view SERVICE_NAME { "netservicedisc" };
	inline static const std::chrono::seconds RETRY_INTERVAL_MIN { 5 };
	inline static const std::chrono::seconds RETRY_INTERVAL_MAX { 120 };

	std::string host_;
	std::uint16_t port_;
	spark::Server& spark_;
	std::atomic_bool connected_;
	boost::asio::io_context ctx_;
	boost::asio::io_context::work work_;
	boost::asio::steady_timer timer_;
	std::jthread worker_;
	log::Logger& logger_;
	spark::Link link_;
	std::chrono::seconds retry_interval_;

	void connect();
	void increase_interval();

	void on_link_up(const spark::Link& link) override;
	void on_link_down(const spark::Link& link) override;
	void connect_failed(std::string_view ip, std::uint16_t port) override;

public:
	NetworkServiceDiscovery(spark::Server& spark, std::string host,
	                        std::uint16_t port, log::Logger& logger);
	~NetworkServiceDiscovery();

	void stop();
};

} // ember