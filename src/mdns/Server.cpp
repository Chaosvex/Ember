 /*
 * Copyright (c) 2021 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Server.h"
#include "Socket.h"
#include "Serialisation.h"
#include "Utility.h"
#include <logger/Logger.h>
#include <spark/buffers/pmr/BufferAdaptor.h>
#include <shared/utility/FormatPacket.h>

namespace ember::dns {

Server::Server(std::unique_ptr<Socket> socket, log::Logger& logger)
               : socket_(std::move(socket)), logger_(logger) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;
    socket_->register_handler(this);
}

Server::~Server() {
	shutdown();
}

void Server::handle_datagram(std::span<const std::uint8_t> datagram) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	const auto result = deserialise(datagram);

	if(!result) {
		LOG_WARN(logger_)
			<< "DNS query deserialising failed: "
			<< to_string(result.error())
			<< LOG_ASYNC;
		return;
	}

	LOG_TRACE(logger_) << to_string(result.value()) << LOG_ASYNC;

	if(result->header.flags.qr == 0) {
		handle_question(result.value());
	} else {
		handle_response(result.value());
	}
}

void Server::handle_question(const Query& query) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;

	Query out{};
	out.questions = query.questions;
	out.header.questions = query.header.questions;
	out.header.id = query.header.id;
	out.header.flags.opcode = Opcode::STANDARD_QUERY;
	out.header.flags.rcode = ReplyCode::REPLY_NO_ERROR;
	out.header.flags.qr = 1;

	if(query.header.answers || query.header.additional_rrs || query.header.authority_rrs) {
		out.header.flags.rcode = ReplyCode::FORMAT_ERROR;
	}

	if(!query.header.questions) {
		// ?
	}

	//auto buffer = std::make_unique<std::vector<std::uint8_t>>();
	//spark::io::pmr::BufferAdaptor adaptor(*buffer);
	//spark::io::pmr::BinaryStream stream(adaptor);
	//parser::serialise(query, stream);
	//socket_->send(std::move(buffer));

	//for (auto& question : query.questions) {
	//	auto it = records_.find({ question.type, question.name });

	//	if (it == records_.end()) {
	//		continue;
	//	}

	//	ResourceRecord record{};
	//	record.name = it->second.name;
	//	record.type = it->second.type;

	//	if (record.type == RecordType::A && it->second.answer.is_v4()) {
	//		Record_A rdata{ it->second.answer.to_v4().to_ulong() };
	//		record.rdata = rdata;
	//	}
	//	else if (record.type == RecordType::AAAA && it->second.answer.is_v6()) {
	//		Record_AAAA rdata{ it->second.answer.to_v6().to_bytes() };
	//		record.rdata = rdata;
	//	}
	//	else {
	//		out.header.flags.reply_code = ReplyCode::SERVER_FAILURE;
	//		break;
	//	}

	//	record.ttl = it->second.ttl;
	//	record.resource_class = Class::CLASS_IN;
	//	out.answers.emplace_back(std::move(record));
	//}

	//out.header.answers = gsl::narrow<std::uint16_t>(out.answers.size());
	//return out;
}

void Server::handle_response(const Query& query) {
	LOG_TRACE(logger_) << log_func << LOG_ASYNC;
}

void Server::shutdown() {
	// todo, broadcast withdrawal of services?
	socket_->close();
}

} // dns, ember