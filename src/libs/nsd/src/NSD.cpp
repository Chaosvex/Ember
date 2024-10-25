/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <nsd/NSD.h>
#include <shared/threading/Utility.h>

namespace ember {

NetworkServiceDiscovery::NetworkServiceDiscovery(spark::Server& spark, std::string host,
                                                 std::uint16_t port, log::Logger& logger)
	: services::DiscoveryClient(spark),
	  host_(std::move(host)), 
	  port_(port),
	  spark_(spark),
	  connected_(false),
	  work_(ctx_),
	  timer_(ctx_),
	  logger_(logger),
      retry_interval_(RETRY_INTERVAL_MIN) {
	worker_ = std::jthread(static_cast<std::size_t(boost::asio::io_context::*)()>
	                       (&boost::asio::io_context::run), &ctx_);
	thread::set_name(worker_, "NSD");
	connect();
}

void NetworkServiceDiscovery::connect() {
	spark_.connect(host_, port_, SERVICE_NAME, this);
}

void NetworkServiceDiscovery::on_link_up(const spark::Link& link) {
	LOG_TRACE_ASYNC(logger_, "Established connection to NSD service");
	link_ = link;
	connected_ = true;
	retry_interval_ = RETRY_INTERVAL_MIN;
}

void NetworkServiceDiscovery::on_link_down(const spark::Link& link) {
	LOG_TRACE_ASYNC(logger_, "Lost connection to NSD service");
	connected_ = false;
}

void NetworkServiceDiscovery::connect_failed(std::string_view ip, std::uint16_t port) {
	LOG_WARN_ASYNC(logger_, "Unable to connect to NSD service, retrying in {}", retry_interval_);

	timer_.expires_from_now(retry_interval_);
	increase_interval();

	timer_.async_wait([&](const boost::system::error_code& ec) {
		if(ec == boost::asio::error::operation_aborted) {
			return;
		}

		connect();
	});
}

void NetworkServiceDiscovery::increase_interval() {
	retry_interval_ *= 2;

	if(retry_interval_ > RETRY_INTERVAL_MAX) {
		retry_interval_ = RETRY_INTERVAL_MAX;
	}
}

void NetworkServiceDiscovery::stop() {
	ctx_.stop();
}

NetworkServiceDiscovery::~NetworkServiceDiscovery() {
	stop();
}

} // ember