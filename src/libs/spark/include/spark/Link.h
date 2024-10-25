/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <cstdint>

namespace ember::spark {

class Channel;

struct Link {
	std::string peer_banner;
	std::string service_name;
	std::weak_ptr<Channel> channel;
};

inline bool operator==(const Link& lhs, const Link& rhs) {
	return lhs.service_name == rhs.service_name;
}

inline bool operator!=(const Link& lhs, const Link& rhs) {
	return !(lhs == rhs);
}

} // spark, ember