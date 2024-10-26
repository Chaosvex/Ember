/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <DiscoveryServiceStub.h>
#include <logger/Logger.h>
#include <spark/Server.h>

namespace ember {

class NSDService final : public services::DiscoveryService {
	log::Logger& logger_;

	void on_link_up(const spark::Link& link) override;
	void on_link_down(const spark::Link& link) override;

public:
	NSDService(spark::Server& spark, log::Logger& logger);
};

} // ember