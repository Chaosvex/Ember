/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <boost/unordered/unordered_flat_map.hpp>
#include <memory>
#include <mutex>
#include <string>

namespace ember::spark {

class RemotePeer;
class Handler;

class Peers final {
	boost::unordered_flat_map<std::string, std::shared_ptr<RemotePeer>> peers_;
	std::mutex lock_;

public:
	void add(std::string key, std::shared_ptr<RemotePeer> peer);
	void remove(const std::string& key);
	std::shared_ptr<RemotePeer> find(const std::string& key);
	void notify_remove_handler(Handler* handler);
};

} // spark, ember