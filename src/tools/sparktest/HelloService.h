/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "HelloServiceStub.h"

namespace ember {

class HelloService final : public services::HelloService {

	std::optional<rpc::Hello::HelloReplyT> handle_say_hello(
		const rpc::Hello::HelloRequest& msg,
	    const spark::Link& link,
	    const spark::Token& token) override;

public:
	HelloService(spark::Server& server);

	void on_link_up(const spark::Link& link) override;
	void on_link_down(const spark::Link& link) override;
};

}