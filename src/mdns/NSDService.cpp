/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "NSDService.h"

using namespace ember::spark;

namespace ember {

NSDService::NSDService(Server& spark, log::Logger& logger)
	: services::DiscoveryService(spark),
	  logger_(logger) {}

void NSDService::on_link_up(const Link& link) {
	LOG_INFO_ASYNC(logger_, "Link up to {}", link.peer_banner);
}

void NSDService::on_link_down(const Link& link) {
	LOG_INFO_ASYNC(logger_, "Link down to {}", link.peer_banner);
}

} // ember