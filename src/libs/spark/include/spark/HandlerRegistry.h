/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <boost/unordered/unordered_flat_map.hpp>
#include <mutex>
#include <string>
#include <vector>

namespace ember::spark {

class Handler;

class HandlerRegistry final {
	boost::unordered_flat_map<std::string, std::vector<Handler*>> services_;
	mutable std::mutex mutex_;

public:
	void register_service(Handler* service);
	void deregister_service(Handler* service);

	Handler* service(const std::string& name) const;
	Handler* service(const std::string& name, const std::string& type) const;
	std::vector<Handler*> services(const std::string& type) const;
	std::vector<std::string> services() const;
};

} // spark, ember