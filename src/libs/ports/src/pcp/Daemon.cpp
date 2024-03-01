/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <ports/pcp/Daemon.h>
#include <algorithm>
#include <random>

namespace ember::ports {

Daemon::Daemon(Client& client, boost::asio::io_context& ctx)
	: client_(client), timer_(ctx), strand_(ctx) {
	daemon_epoch_ = std::chrono::steady_clock::now();

	client_.announce_handler(strand_.wrap([&](std::uint32_t epoch) {
		check_epoch(epoch);
	}));
	
	start_renew_timer();
}

void Daemon::start_renew_timer() {
	timer_.expires_from_now(TIMER_INTERVAL);
	timer_.async_wait(strand_.wrap([&](const boost::system::error_code& ec) {
		if(ec) {
			return;
		}

		const auto time = std::chrono::steady_clock::now();

		for(auto& mapping : mappings_) {
			if((mapping.expiry - time) < RENEW_WHEN_BELOW) {
				queue_.emplace(mapping);
			}
		}

		process_queue();
	}));
}

void Daemon::process_queue() {
	timer_.cancel();

	if(queue_.empty()) {
		start_renew_timer();
		return;
	}
	const auto mapping = queue_.front();
	queue_.pop();
	renew_mapping(mapping);
}

void Daemon::renew_mapping(const Mapping& mapping) {
	client_.add_mapping(mapping.request, strand_.wrap([&](const Result& result) {
		if(result) {
			update_mapping(result);
		}

		process_queue();
	}));
}

void Daemon::update_mapping(const Result& result) {
	for(auto& mapping : mappings_) {
		if(mapping.request.internal_port == result->internal_port) {
			const auto time = std::chrono::steady_clock::now();
			mapping.expiry = time + std::chrono::seconds(result->lifetime);
		}
	}
}

void Daemon::renew_mappings() {
	for(const auto& mapping : mappings_) {
		queue_.emplace(mapping);
	}

	process_queue();
}

void Daemon::check_epoch(std::uint32_t epoch) {
	// if we haven't received anything yet, we can't compare previous epoch
	if(!epoch_acquired_) {
		gateway_epoch_ = epoch;
		epoch_acquired_ = true;
		return;
	}

	// if epoch is less than recorded time, gateway has (likely) dropped mappings
	if(epoch < gateway_epoch_) {
		gateway_epoch_ = epoch;
		renew_mappings();
		return;
	}

	// does anybody actually like std::chrono?
	const auto daemon_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::steady_clock::now() - daemon_epoch_
	);
	
	// RFC 6886 - 3.6.  Seconds Since Start of Epoch
	const std::chrono::seconds gs {gateway_epoch_};
	const auto expected_epoch = gs + (daemon_elapsed * 0.875);
	const std::chrono::seconds es {epoch};
	gateway_epoch_ = epoch;

	if(es < (expected_epoch - 2s)) {
		renew_mappings();
	}
}

void Daemon::add_mapping(MapRequest request, RequestHandler&& handler) {
	// If there's no ID set, we'll set our own and make sure we keep hold of it
	// for refreshes (spec requires it to match OG request to refresh)
	const auto it = std::find_if(request.nonce.begin(), request.nonce.end(),
		[](const std::uint8_t val) {
			return val != 0;
		});

	if(it == request.nonce.end()) {
;		std::random_device engine;
		std::generate(request.nonce.begin(), request.nonce.end(), std::ref(engine));
	}

	RequestHandler wrapped_handler =
		[&, request, handler = std::move(handler)](const Result& result) {
		strand_.dispatch([&, request] {
			if(result) {
				Mapping mapping{};
				mapping.request = request;
				mapping.expiry = std::chrono::steady_clock::now()
					+ std::chrono::seconds(result->lifetime);

				mappings_.emplace_back(std::move(mapping));
			}
		});

		handler(result);
	};

	client_.add_mapping(request, std::move(wrapped_handler));
}

void Daemon::delete_mapping(std::uint16_t internal_port, Protocol protocol,
							RequestHandler&& handler) {
	RequestHandler wrapped_handler = 
		[&, handler = std::move(handler)](const Result& result) {
		if(result) {
			strand_.dispatch([&, r = result] {
				erase_mapping(r);
			});
		}

		handler(result);
	};

	client_.delete_mapping(internal_port, protocol, std::move(wrapped_handler));
}

/*
 * Not using a map here because there's no ideal key that
 * wouldn't make the API more awkward to use. 'internal_port'
 * would be a good candidate but it fails if a router allows
 * multiple ext. port -> same int. port mappings (test HW didn't)
 * - don't want a map of vectors
 * 
 * Other option might be the PCP ID but we also support NAT-PMP (no IDs)
 * and a delete request does not need to be 1-to-1 with an add request,
 * so we can't ask the user to carry an ID around
 * 
 * Iteration speed for refreshes is more important here, so this'll do
 */
void Daemon::erase_mapping(const Result& result) {
	for(auto it = mappings_.begin(); it != mappings_.end();) {
		if(it->request.internal_port == result->internal_port) {
			it = mappings_.erase(it);
		} else {
			++it;
		}
	}
}

} // natpmp, ports, ember
