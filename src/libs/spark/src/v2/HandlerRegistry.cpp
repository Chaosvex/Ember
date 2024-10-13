/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <spark/v2/HandlerRegistry.h>
#include <spark/v2/Handler.h>
#include <ranges>

namespace ember::spark::v2 {

void HandlerRegistry::deregister_service(Handler* service) {
	std::lock_guard guard(mutex_);
	
	auto it = services_.find(service->type());

	if(it == services_.end()) {
		return;
	}

	auto& [_, handlers] = *it;

	for(auto i = handlers.begin(); i != handlers.end(); ++i) {
		if(*i == service) {
			handlers.erase(i);
			break;
		}
	}

	if(handlers.empty()) {
		services_.erase(service->type());
	}
}

void HandlerRegistry::register_service(Handler* service) {
	std::lock_guard guard(mutex_);
	auto type = service->type();
	services_[type].emplace_back(service);
}

Handler* HandlerRegistry::service(const std::string& name) const {
	std::lock_guard guard(mutex_);

	for(auto& [_, v] : services_) {
		for(auto& service : v) {
			if(service->name() == name) {
				return service;
			}
		}
	}

	return nullptr;
}


Handler* HandlerRegistry::service(const std::string& name, const std::string& type) const {
	std::lock_guard guard(mutex_);

	auto it = services_.find(type);

	if(it == services_.end()) {
		return nullptr;
	}

	auto& [_, services] = *it;

	for(auto& service : services) {
		if(service->name() == name) {
			return service;
		}
	}

	return nullptr;
}

std::vector<Handler*> HandlerRegistry::services(const std::string& type) const {
	std::lock_guard guard(mutex_);
	return services_.at(type);
}

std::vector<std::string> HandlerRegistry::services() const {
	std::lock_guard guard(mutex_);
	auto view = services_ | std::views::keys;
	return std::ranges::to<std::vector>(view);
}

} // v2, spark, ember