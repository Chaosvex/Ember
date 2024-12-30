/*
 * Copyright (c) 2018 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ConsoleSink.h"
#include <protocol/Opcodes.h>
#include <shared/utility/FormatPacket.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstring>

namespace ember {

void ConsoleSink::handle(const fblog::Header& header) {
	std::cout << "<header>\n";
	std::cout << "Local address: ";

	if(header.host()) {
		std::cout << header.host()->c_str();
	} else {
		std::cout << "<missing>";
	}

	std::cout << "\nNode: ";

	if(header.host_desc()) {
		std::cout << header.host_desc()->c_str();
	} else {
		std::cout << "<missing>";
	}

	std::cout << "\nRemote host: ";

	if(header.remote_host()) {
		std::cout << header.remote_host()->c_str();
	} else {
		std::cout << "<missing>";
	}

	std::cout << "\nTime format: ";

	if(header.time_format()) {
		std::cout << header.time_format()->c_str();
	} else {
		std::cout << "<missing>";
	}

	std::cout << "\n</header>";
	std::cout << "\n\n";
}

void ConsoleSink::handle(const fblog::Message& message) {
	std::cout << "<message>\n";

	if(message.time()) {
		std::tm time;
		std::istringstream ss(message.time()->c_str());
		ss >> std::get_time(&time, time_fmt_);
		std::cout << std::put_time(&time, "%a, %B %d, %Y @ %H:%M:%S UTC\n");
	} else {
		std::cout << "<missing time>\n";
	}

	print_opcode(message);

	if(!message.payload()) {
		std::cout << "<missing payload>\n";
		return;
	}
	
	const auto payload = message.payload();

	std::cout << util::format_packet(payload->data(), payload->size());
	std::cout << "\n</message>\n" << std::endl; // explicit flush to avoid stalls for ongoing streams
}

void ConsoleSink::print_opcode(const fblog::Message& message) const {
	protocol::ClientOpcode c_op;
	protocol::ServerOpcode s_op;
	std::string op_desc;
	const auto payload = message.payload();

	if(!payload) {
		std::cout << "<missing opcode>\n";
		return;
	}

	switch(message.direction()) {
		case fblog::Direction::INBOUND:
			if(payload->size() < sizeof(c_op)) {
				std::cout << "<bad payload>\n";
				return;
			}

			std::memcpy(&c_op, payload->data(), sizeof(c_op));
			op_desc = protocol::to_string(c_op);
			break;
		case fblog::Direction::OUTBOUND:
			if(payload->size() < sizeof(s_op)) {
				std::cout << "<bad payload>\n";
				return;
			}

			std::memcpy(&s_op, payload->data(), sizeof(s_op));
			op_desc = protocol::to_string(s_op);
			break;
		default:
			throw std::runtime_error("Unknown message direction");
	}

	std::cout << op_desc << "\n";
} 

} // ember