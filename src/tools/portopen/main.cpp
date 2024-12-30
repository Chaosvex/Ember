/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <ports/pcp/Client.h>
#include <ports/upnp/SSDP.h>
#include <ports/upnp/IGDevice.h>
#include <shared/utility/polyfill/print>
#include <boost/asio/ip/address.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <cstdint>
#include <cstdlib>

namespace po = boost::program_options;

using namespace ember;

void launch(const po::variables_map& args);
po::variables_map parse_arguments(int argc, const char* argv[]);
void print_error(const ports::Error& error);
void use_upnp(const po::variables_map& args);
void use_natpmp(const po::variables_map& args);

int main(int argc, const char* argv[]) try {
	const po::variables_map args = parse_arguments(argc, argv);
	launch(args);
	return EXIT_SUCCESS;
} catch(const std::exception& e) {
	std::cerr << e.what();
	return EXIT_FAILURE;
}

void launch(const po::variables_map& args) {
	if(args.contains("upnp")) {
		use_upnp(args);
	} else {
		use_natpmp(args);
	}
}

void print_error(const ports::Error& error) {
	std::println("Mapping error: {} ({})", error.code, std::to_underlying(error.code));

	if(error.code == ports::ErrorCode::PCP_CODE) {
		std::println("PCP code: {}", error.pcp_code);
	} else if(error.code == ports::ErrorCode::NATPMP_CODE) {
		std::println("NAT-PMP code: {}", error.natpmp_code);
	}
}

void use_natpmp(const po::variables_map& args) {
	const auto internal = args["internal"].as<std::uint16_t>();
	const auto external = args["external"].as<std::uint16_t>();
	const auto& interface = args["interface"].as<std::string>();
	const auto& gateway = args["gateway"].as<std::string>();
	const auto& protocol = args["protocol"].as<std::string>();
	const auto deletion = args.contains("delete");
	
	auto proto = ports::Protocol::TCP;

	if(protocol == "udp") {
		proto = ports::Protocol::UDP;
	}

	const ports::MapRequest request {
		.protocol = proto,
		.internal_port = internal,
		.external_port = external,
		.lifetime = deletion? 0u : 7200u
	};

	boost::asio::io_context ctx;

	ports::Client client(interface, gateway, ctx);

	// Create an ASIO worker with a single thread
	auto worker = std::jthread(
		static_cast<size_t(boost::asio::io_context::*)()>(&boost::asio::io_context::run), &ctx
	);

	std::future<ports::Result> future;

	if(deletion) {
		future = client.delete_mapping(internal, proto);
	} else {
		future = client.add_mapping(request, true);
	}

	auto result = future.get();

	if(result) {
		std::println("Successful {}: {} -> {} for {} seconds",
		            deletion? "deletion" : "mapping",
		            result->external_port,
		            result->internal_port,
		            result->lifetime);
	} else {
		std::cout << "Error: could not map port" << std::endl;
		print_error(result.error());
	}

	auto xfuture = client.external_address();
	const auto xresult = xfuture.get();

	if(xresult) {
		const auto v6 = boost::asio::ip::address_v6(xresult->external_ip);
		std::print("External address: {}", v6.to_string());
	} else {
		std::cout << "Error: could not retrieve external address" << std::endl;
		print_error(xresult.error());
	}

	ctx.stop();
}

void use_upnp(const po::variables_map& args) {
	const auto& interface = args["interface"].as<std::string>();
	const auto& protocol = args["protocol"].as<std::string>();
	const auto internal = args["internal"].as<std::uint16_t>();
	const auto external = args["external"].as<std::uint16_t>();
	const auto deletion = args.contains("delete");

	boost::asio::io_context ctx;
	ports::upnp::SSDP ssdp(interface, ctx);

	auto proto = ports::Protocol::TCP;

	if(protocol == "udp") {
		proto = ports::Protocol::UDP;
	}

	ssdp.locate_gateways([&](ports::upnp::LocateResult result) {
		if(!result) {
			std::cout << result.error() << '\n';
			return true;
		}

		ports::upnp::Mapping map {
			.external = external,
			.internal = internal,
			.ttl = 0,
			.protocol = proto
		};

		auto callback = [&, map](ports::upnp::ErrorCode ec) {
			if(!ec) {
				std::println("Port {} {} mapping successfully using UPnP",
				           map.external, deletion? "delete" : "add");
			} else {
				std::println("Port {} {} failed using UPnP, error {}",
				           map.external, deletion? "delete" : "add",
				           ec.value());
			}
		};
		
		if(deletion) {
			result->device->delete_port_mapping(map, callback);
		} else {
			result->device->add_port_mapping(map, callback);
		}

		return false;
	});

	ctx.run();
}

po::variables_map parse_arguments(int argc, const char* argv[]) {
	po::options_description cmdline_opts("Options");
	cmdline_opts.add_options()
		("help", "Displays a list of available options")
		("upnp", "Use UPnP rather than NAT-PMP/PCP")
		("internal,i", po::value<std::uint16_t>()->default_value(8085), "Internal port")
		("external,x", po::value<std::uint16_t>()->default_value(8085), "External port")
		("interface,f", po::value<std::string>()->default_value("0.0.0.0"), "Interface to bind to")
		("gateway,g", po::value<std::string>()->default_value(""), "Gateway address")
		("delete,d", "Delete mapping")
		("protocol,p", po::value<std::string>()->default_value("udp"), "Protocol (udp, tcp)");

	po::variables_map options;
	po::store(po::command_line_parser(argc, argv).options(cmdline_opts).run(), options);

	if(options.count("help")) {
		std::cout << cmdline_opts;
		std::exit(EXIT_SUCCESS);
	}

	po::notify(options);

	return options;
}