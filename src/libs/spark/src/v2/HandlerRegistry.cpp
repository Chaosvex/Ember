/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <spark/v2/HandlerRegistry.h>
#include <spark/v2/Handler.h>

namespace ember::spark::v2 {

void HandlerRegistry::deregister_service(Handler* service) {
	std::lock_guard<std::mutex> guard(mutex_);
	
	if(!services_.contains(service->type())) {
		return;
	}

	auto& handlers = services_.at(service->type());

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
	std::lock_guard<std::mutex> guard(mutex_);
	auto type = service->type();
	services_[type].emplace_back(service);
}

Handler* HandlerRegistry::service(const std::string& name) const {
	std::lock_guard<std::mutex> guard(mutex_);

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
	std::lock_guard<std::mutex> guard(mutex_);

	if(!services_.contains(type)) {
		return nullptr;
	}

	auto& services = services_.at(type);

	for(auto& service : services) {
		if(service->name() == name) {
			return service;
		}
	}

	return nullptr;
}

std::vector<Handler*> HandlerRegistry::services(const std::string& type) const {
	std::lock_guard<std::mutex> guard(mutex_);
	return services_.at(type);
}

std::vector<std::string> HandlerRegistry::services() const {
	std::lock_guard<std::mutex> guard(mutex_);
	std::vector<std::string> types;

	for(auto& [k, _]: services_) {
		types.emplace_back(k);
	}

	return types;
}

} // v2, spark, ember