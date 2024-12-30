/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "LogConfig.h"
#include <logger/ConsoleSink.h>
#include <logger/FileSink.h>
#include <logger/SyslogSink.h>
#include <logger/Utility.h>
#include <string>
#include <stdexcept>
#include <utility>
#include <cstdint>

namespace po = boost::program_options;

namespace ember::util {

namespace {

std::unique_ptr<log::Sink> init_remote_sink(const po::variables_map& args, log::Severity severity) {
	const auto& host = args["remote_log.host"].as<std::string>();
	const auto& service = args["remote_log.service_name"].as<std::string>();
	auto port = args["remote_log.port"].as<std::uint16_t>();
	auto facility = log::SyslogSink::Facility::LOCAL_USE_0;
	auto filter = args["remote_log.filter-mask"].as<std::uint32_t>();
	return std::make_unique<log::SyslogSink>(severity, log::Filter(filter), host, port, facility, service);
}

std::unique_ptr<log::Sink> init_file_sink(const po::variables_map& args, log::Severity severity) {
	const auto& mode_str = args["file_log.mode"].as<std::string>();
	const auto& path = args["file_log.path"].as<std::string>();
	auto filter = args["file_log.filter-mask"].as<std::uint32_t>();

	if(mode_str != "append" && mode_str != "truncate") {
		throw std::runtime_error("Invalid file logging mode supplied");
	}

	auto mode = (mode_str == "append")? log::FileSink::Mode::APPEND :
	                                    log::FileSink::Mode::TRUNCATE;

	auto sink = std::make_unique<log::FileSink>(severity, log::Filter(filter), path, mode);
	sink->size_limit( args["file_log.size_rotate"].as<std::uint32_t>());
	sink->log_severity(args["file_log.log_severity"].as<bool>());
	sink->log_date(args["file_log.log_timestamp"].as<bool>());
	sink->time_format(args["file_log.timestamp_format"].as<std::string>());
	sink->midnight_rotate(args["file_log.midnight_rotate"].as<bool>());
	return sink;
}

std::unique_ptr<log::Sink> init_console_sink(const po::variables_map& args, log::Severity severity) {
	auto filter = args["console_log.filter-mask"].as<std::uint32_t>();
	auto colourise = args["console_log.colours"].as<bool>();
	auto sink = std::make_unique<log::ConsoleSink>(severity, log::Filter(filter));
	sink->colourise(colourise);

	if(args.count("console_log.prefix")) {
		sink->prefix(args["console_log.prefix"].as<std::string>());
	}

	return sink;
}

} // unnamed

void configure_logger(log::Logger& logger, const po::variables_map& args) {
	log::Severity severity;

	if((severity = log::severity_string(args["console_log.verbosity"].as<std::string>()))
		!= log::Severity::DISABLED) {
		logger.add_sink(init_console_sink(args, severity));
	}

	if((severity = log::severity_string(args["file_log.verbosity"].as<std::string>()))
		!= log::Severity::DISABLED) {
		logger.add_sink(init_file_sink(args, severity));
	}

	if((severity = log::severity_string(args["remote_log.verbosity"].as<std::string>()))
		!= log::Severity::DISABLED) {
		logger.add_sink(init_remote_sink(args, severity));
	}
}

} // util, ember