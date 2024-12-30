/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <logger/Logger.h>
#include <ports/pcp/Client.h>
#include <ports/pcp/Daemon.h>
#include <ports/upnp/SSDP.h>
#include <ports/upnp/IGDevice.h>
#include <boost/asio/io_context.hpp>
#include <atomic>
#include <memory>
#include <semaphore>
#include <string>
#include <cassert>
#include <cstdint>

namespace ember::util {

/*
 * Helper class that provides a basic API for forwarding a port for
 * any services that may need to be available over the Internet.
 *
 * This is a best-effort service in that it makes no guarantee that
 * it'll work reliably given the hardware variety and implementation
 * quality of the various protocols.
 */
class PortForward final {
	boost::asio::io_context& ctx_;
	log::Logger& logger_;
	std::uint16_t port_;
	std::atomic_bool mapping_active_;
	std::unique_ptr<ports::Client> client_;
	std::unique_ptr<ports::Daemon> daemon_;
	std::unique_ptr<ports::upnp::SSDP> ssdp_;
	std::shared_ptr<ports::upnp::IGDevice> upnp_device_;
	std::binary_semaphore cb_sem_, sem_;

	void start_upnp(const std::string& iface, std::uint16_t port) {
		ssdp_ = std::make_unique<ports::upnp::SSDP>(iface, ctx_);

		ssdp_->locate_gateways([&, port](ports::upnp::LocateResult result) {
			if(!result) {
				LOG_ERROR_ASYNC(logger_, "UPnP gateway search failed with error code {}",
				                result.error().value());
				cb_sem_.release();
				return true;
			}

			const ports::upnp::Mapping map {
				.external = port,
				.internal = port,
				.ttl = 0,
				.protocol = ports::Protocol::TCP
			};

			result->device->add_port_mapping(map, [&](ports::upnp::ErrorCode ec) {
				if(!ec) {
					LOG_INFO_ASYNC(logger_, "Port {} successfully forwarded (UPnP)", port);
					mapping_active_ = true;
				} else {
					LOG_ERROR_ASYNC(logger_, "Port forwarding failed (UPnP), error {}", ec.value());
				}

				cb_sem_.release();
			});

			upnp_device_ = std::move(result->device);
			return false;
		});
	}

	void start_pmp(const std::string& iface, const std::string& gateway, std::uint16_t port) {
		const ports::MapRequest request {
			.protocol      = ports::Protocol::TCP,
			.internal_port = port,
			.external_port = port,
			.lifetime      = 7200u
		};

		client_ = std::make_unique<ports::Client>(iface, gateway, ctx_);
		daemon_ = std::make_unique<ports::Daemon>(*client_, ctx_);

		daemon_->add_mapping(request, false, [&](const ports::Result& result) {
			if(result) {
				LOG_INFO_ASYNC(logger_, "Port {} -> {} forwarded for {} seconds (NATPMP/PCP)",
				               result->external_port,
				               result->internal_port,
				               result->lifetime);
				mapping_active_ = true;
			} else {
				LOG_ERROR_ASYNC(logger_, "Port forwarding failed (NATPMP/PCP), error {}",
				                result.error().code);
				mapping_active_ = false; // mapping can succeed and later fail when refreshed
			}

			cb_sem_.release();
		});
	}

	void unmap_upnp() {
		assert(upnp_device_);

		const ports::upnp::Mapping map {
			.external = port_,
			.internal = port_,
			.ttl = 0,
			.protocol = ports::Protocol::TCP
		};

		upnp_device_->delete_port_mapping(map, [&](ports::upnp::ErrorCode ec) {
			if(!ec) {
				LOG_INFO_ASYNC(logger_, "Successfully unmapped forwarded port {}", port_);
			} else {
				LOG_ERROR_ASYNC(logger_, "Failed to unmap forwarded port {}, error ", ec.value());
			}

			sem_.release();
		});

		sem_.acquire();
	}

	void unmap_pmp() {
		assert(daemon_);

		daemon_->delete_mapping(port_, ports::Protocol::TCP, [&](const ports::Result& result) {
			if(result) {
				LOG_INFO_ASYNC(logger_, "Successfully unmapped forwarded port", port_);
			} else {
				LOG_ERROR_ASYNC(logger_, "Failed to unmap forwarded port, error {}", result.error().code);
			}

			sem_.release();
		});

		sem_.acquire();
	}

	void start_auto(const std::string& iface, const std::string& gateway, std::uint16_t port) {
		ctx_.post([=, this]() mutable {
			start_upnp(iface, port);
			cb_sem_.acquire();

			if(mapping_active_) {
				return;
			}

			start_pmp(iface, gateway, port);
			cb_sem_.acquire();

			if(!mapping_active_) {
				LOG_WARN_ASYNC(logger_, "Automatic port forwarding not permitted by NAT gateway "
				                        "or device does not support any forwarding protocols");
			}
		});
	}

public:
	enum class Mode {
		UPNP, PMP_PCP, AUTO
	};

	PortForward(log::Logger& logger, boost::asio::io_context& ctx, Mode mode,
				const std::string& iface, const std::string& gateway, std::uint16_t port)
		: ctx_(ctx), logger_(logger), port_(port), mapping_active_(false), cb_sem_(0), sem_(0) {
		switch(mode) {
			case Mode::UPNP:
				start_upnp(iface, port);
				break;
			case Mode::PMP_PCP:
				start_pmp(iface, gateway, port);
				break;
			case Mode::AUTO:
				start_auto(iface, gateway, port);
				break;
			default:
				assert("Bad port forwarding method specified");
		}
	}

	~PortForward() {
		unmap();
	}

	void unmap() {
		if(!mapping_active_) {
			return;
		}

		if(daemon_) {
			unmap_pmp();
		} else if(upnp_device_) {
			unmap_upnp();
		}

		// if it failed, too bad, nothing we can do
		mapping_active_ = false;
	}
};

} // util, ember