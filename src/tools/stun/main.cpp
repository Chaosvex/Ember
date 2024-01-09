/*
 * Copyright (c) 2023 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stun/Client.h>

#include <boost/program_options.hpp>
#include <format>
#include <iostream>
#include <cstdint>

namespace po = boost::program_options;

using namespace ember;

void launch(const po::variables_map& args);
po::variables_map parse_arguments(int argc, const char* argv[]);

int main(int argc, const char* argv[]) try {
	const po::variables_map args = parse_arguments(argc, argv);
	launch(args);
} catch(const std::exception& e) {
	std::cerr << e.what();
	return 1;
}

void launch(const po::variables_map& args) {
	const auto host = args["host"].as<std::string>();
	const auto port = args["port"].as<std::uint16_t>();
	const auto protocol = args["protocol"].as<std::string>();
	
	auto proto = stun::Protocol::UDP;

	if (protocol == "tcp") {
		proto = stun::Protocol::TCP;
	} else if (protocol == "tls_tcp") {
		proto = stun::Protocol::TLS_TCP;
	} else if (protocol != "udp") {
		throw std::invalid_argument("Unknown protocol specified");
	}

	// todo, std::print when supported
	std::cout << std::format("Connecting to {}:{} ({})...", host, port, protocol);

	stun::Client client;
	client.connect(host, port, proto);
	std::future<std::string> result = client.mapped_address();
	
	// todo, std::print when supported
	//std::cout << std::format("STUN provider returned our address as {}", result.get());

	while (1) _sleep(500);
}

po::variables_map parse_arguments(int argc, const char* argv[]) {
	po::options_description cmdline_opts("Options");
	cmdline_opts.add_options()
		("host,h", po::value<std::string>()->default_value("stun.l.google.com"), "Host")
		("port,p", po::value<std::uint16_t>()->default_value(19302), "Port")
		("protocol,c", po::value<std::string>()->default_value("udp"), "Protocol (udp, tcp, tls_tcp)");

	po::variables_map options;
	po::store(po::command_line_parser(argc, argv).options(cmdline_opts).run(), options);

	if(options.count("help")) {
		std::cout << cmdline_opts << "\n";
		std::exit(0);
	}

	po::notify(options);

	return options;
}