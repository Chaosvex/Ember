/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Event.h"
#include "ClientHandler.h"
#include <shared/threading/ServicePool.h>
#include <shared/ClientRef.h>
#include <boost/unordered/unordered_flat_map.hpp>
#include <concepts>
#include <memory>
#include <vector>

namespace ember {

class EventDispatcher final {
	using HandlerMap = boost::unordered_flat_map<
		ClientRef, ClientHandler*, boost::hash<ClientRef>
	>;

	const ServicePool& pool_;
	static inline thread_local HandlerMap handlers_;

public:
	explicit EventDispatcher(const ServicePool& pool) : pool_(pool) {}

	void exec(const ClientRef& client, auto work) const {
		auto service = pool_.get_if(client.service());

		// bad service index encoded in the UUID
		if(service == nullptr) {
			LOG_ERROR_GLOB << "Invalid service index, " << client.service() << LOG_ASYNC;
			return;
		}

		service->post([client, work = std::move(work)] {
			if(!handlers_.contains(client)) {
				LOG_DEBUG_GLOB << "Client disconnected, work discarded" << LOG_ASYNC;
				return;
			}

			work();
		});
	}

	auto post_event(const ClientRef& client, std::derived_from<Event> auto event) const {
		auto service = pool_.get_if(client.service());

		// bad service index encoded in the UUID
		if(service == nullptr) {
			LOG_ERROR_GLOB << "Invalid service index, " << client.service() << LOG_ASYNC;
			return;
		}

		service->post([&, client, event = std::move(event)] {
			if(auto it = handlers_.find(client); it != handlers_.end()) {
				auto& [_, handler] = *it;
				handler->handle_event(&event);
			} else {
				LOG_DEBUG_GLOB << "Client disconnected, event discarded" << LOG_ASYNC;
			}
		});
	}

	void post_event(const ClientRef& client, std::unique_ptr<Event> event) const;
	void broadcast_event(std::vector<ClientRef> clients, std::shared_ptr<const Event> event) const;
	void register_handler(ClientHandler* handler);
	void remove_handler(const ClientHandler* handler);
};

} // ember
