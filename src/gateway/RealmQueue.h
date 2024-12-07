/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <shared/ClientRef.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <cstddef>

namespace ember::gateway {

class ClientConnection;
using namespace std::chrono_literals;

class RealmQueue final {
	using LeaveQueueCB = std::function<void()>;
	using UpdateQueueCB = std::function<void(std::size_t)>;

	struct QueueEntry {
		int priority;
		ClientRef client;
		UpdateQueueCB on_update;
		LeaveQueueCB on_leave;

		bool operator>(const QueueEntry& rhs) const {
			return rhs.priority > priority;
		}

		bool operator<(const QueueEntry& rhs) const {
			return rhs.priority < priority;
		}
	};

	static constexpr auto DEFAULT_FREQUENCY { 250ms };
	const std::chrono::milliseconds frequency_;

	boost::asio::steady_timer timer_;
	std::list<QueueEntry> queue_;
	std::mutex lock_;
	bool dirty_;

	void update_clients();
	void set_timer();

public:
	explicit RealmQueue(boost::asio::io_context& service,
	                    std::chrono::milliseconds frequency = DEFAULT_FREQUENCY)
	                    : timer_(service), frequency_(frequency), dirty_(false) { }

	void enqueue(ClientRef client, UpdateQueueCB on_update_cb,
	             LeaveQueueCB on_leave_cb, int priority = 0);
	void dequeue(const ClientRef& client);
	void free_slot();
	void shutdown();
	std::size_t size() const;
};

} // gateway, ember