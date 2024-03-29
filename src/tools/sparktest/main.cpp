/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "HelloService.h"
#include <logger/Logging.h>
#include <logger/ConsoleSink.h>
#include <logger/FileSink.h>
#include <spark/v2/Server.h>
#include <boost/asio.hpp>
#include <memory>

using namespace ember;

void init_logger(log::Logger* logger);

int main() {
	auto logger = std::make_unique<log::Logger>();
	init_logger(logger.get());

	boost::asio::io_context ctx;
	spark::v2::Server spark(ctx, "0.0.0.0", 8000, logger.get());
	spark::v2::Server spark_cli(ctx, "0.0.0.0", 8001, logger.get());

	HelloService hello_service(spark);

	spark_cli.connect("127.0.0.1", 8000);
	ctx.run();
}

void init_logger(log::Logger* logger) {
	const auto& con_verbosity = log::severity_string("trace");
	const auto& file_verbosity = log::severity_string("trace");

	auto consink = std::make_unique<log::ConsoleSink>(con_verbosity, log::Filter(0));
	consink->colourise(true);
	logger->add_sink(std::move(consink));
	log::set_global_logger(logger);
}