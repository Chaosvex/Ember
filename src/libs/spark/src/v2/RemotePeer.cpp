/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <spark/v2/RemotePeer.h>
#include <spark/v2/Message.h>
#include <spark/v2/buffers/BufferAdaptor.h>
#include <spark/v2/buffers/BinaryStream.h>
#include <spark/v2/HandlerRegistry.h>
#include <spark/Exception.h>
#include <shared/FilterTypes.h>
#include <gsl/gsl_util>

namespace ba = boost::asio;

namespace ember::spark::v2 {

RemotePeer::RemotePeer(ba::ip::tcp::socket socket, HandlerRegistry& registry, log::Logger* log)
	: handler_(*this),
	  registry_(registry),
	  conn_(*this, std::move(socket)),
	  log_(log) {
}

void RemotePeer::write_header(Message& msg) {
	MessageHeader header;
	header.size = msg.fbb.GetSize();
	header.set_alignment(msg.fbb.GetBufferMinAlignment());

	BufferAdaptor adaptor(msg.header);
	BinaryStream stream(adaptor);
	header.write_to_stream(stream);
}

ba::awaitable<void> RemotePeer::send_banner(const std::string& banner) {
	LOG_TRACE(log_) << __func__ << LOG_ASYNC;

	core::HelloT hello;
	hello.description = banner;

	Message msg;
	finish(hello, msg);
	write_header(msg);
	co_await conn_.send(msg);
}

ba::awaitable<std::string> RemotePeer::receive_banner() {
	LOG_TRACE(log_) << __func__ << LOG_ASYNC;

	auto msg = co_await conn_.receive_msg();

	spark::v2::BufferAdaptor adaptor(msg);
	spark::v2::BinaryStream stream(adaptor);

	MessageHeader header;

	if(header.read_from_stream(stream) != MessageHeader::State::OK
	   || header.size <= stream.total_read()) {
		throw exception("bad message header");
	}

	const auto header_size = stream.total_read();
	std::span flatbuffer(msg.data() + header_size, msg.size_bytes() - header_size);

	flatbuffers::Verifier verifier(flatbuffer.data(), flatbuffer.size());
	auto fb = core::GetHeader(flatbuffer.data());
	auto hello = fb->message_as_Hello();

	if(!hello->Verify(verifier)) {
		throw exception("bad flatbuffer message");
	}

	co_return hello->description()->str();
}

template<typename T>
void RemotePeer::finish(T& payload, Message& msg) {
	core::HeaderT header_t;
	core::MessageUnion mu;
	mu.Set(payload);
	header_t.message = mu;
	msg.fbb.Finish(core::Header::Pack(msg.fbb, &header_t));
}

void RemotePeer::send(std::unique_ptr<Message> msg) {
	write_header(*msg);
	conn_.send(std::move(msg));
}

void RemotePeer::receive(std::span<const std::uint8_t> data) {
	LOG_TRACE(log_) << __func__ << LOG_ASYNC;

	spark::v2::BufferAdaptor adaptor(data);
	spark::v2::BinaryStream stream(adaptor);

	MessageHeader header;

	if(header.read_from_stream(stream) != MessageHeader::State::OK
	   || header.size <= stream.total_read()) {
		LOG_WARN_FILTER(log_, LF_SPARK)
			<< "[spark] Bad message from "
			<< conn_.address()
			<< LOG_ASYNC;
		return;
	}

	const auto header_size = stream.total_read();
	std::span flatbuffer(data.data() + header_size, data.size_bytes() - header_size);

	if(header.channel == 0) {
		handle_control_message(flatbuffer);
	} else {
		handle_channel_message(header, flatbuffer);
	}
}

void RemotePeer::handle_open_channel_response(const core::OpenChannelResponse* msg) {
	LOG_TRACE(log_) << __func__ << LOG_ASYNC;

	if(msg->result() != core::Result::OK) {
		channels_[msg->requested_id()].reset();
		send_close_channel(msg->actual_id());
		return;
	}

	// todo, index bounds checks
	auto id = msg->actual_id();
	Channel& channel = channels_[id];

	if(msg->actual_id() != msg->requested_id()) {
		if(channel.state() != Channel::State::EMPTY) {
			send_close_channel(msg->actual_id());
			channels_[msg->requested_id()].reset();
			return;
		}

		channels_[msg->actual_id()] = channels_[msg->requested_id()];
		channels_[msg->requested_id()].reset();
	}

	if(channel.state() != Channel::State::HALF_OPEN) {
		send_close_channel(msg->actual_id());
		channels_[msg->actual_id()].reset();
		return;
	}

	channel.state(Channel::State::OPEN);
	LOG_INFO_FMT(log_, "[spark] Remote channel open, {}:{}",
		channel.handler()->type(), msg->actual_id());
}

void RemotePeer::send_close_channel(const std::uint8_t id) {
	LOG_TRACE(log_) << __func__ << LOG_ASYNC;

	core::CloseChannelT body;
	body.channel = id;

	auto msg = std::make_unique<Message>();
	finish(body, *msg);
	write_header(*msg);
	conn_.send(std::move(msg));
}

void RemotePeer::handle_open_channel(const core::OpenChannel* msg) {
	LOG_TRACE(log_) << __func__ << LOG_ASYNC;

	const auto service = msg->service()->str();
	auto handlers = registry_.services(service);

	// todo, change how the registry works
	if(handlers.empty()) {
		LOG_DEBUG_FMT(log_, "[spark] Requested service ({}) does not exist", service);
		open_channel_response(core::Result::ERROR, 0, msg->id());
		return;
	}

	if(msg->id() == 0 || msg->id() >= channels_.size()) {
		LOG_DEBUG_FMT(log_, "[spark] Bad channel ID ({}) specified", msg->id());
		open_channel_response(core::Result::ERROR, 0, msg->id());
		return;
	}

	auto id = gsl::narrow<std::uint8_t>(msg->id());
	auto& channel = channels_[id];

	if(channel.state() != Channel::State::EMPTY) {
		if(id = next_empty_channel(id); id == 0) {
			LOG_ERROR_FMT(log_, "[spark] Exhausted channel IDs");
			open_channel_response(core::Result::ERROR, 0, msg->id());
			return;
		}
	}

	channel.state(Channel::State::OPEN);
	channel.handler(handlers[0]); // todo
	open_channel_response(core::Result::OK, id, msg->id());
	LOG_INFO_FMT(log_, "[spark] Remote channel open, {}:{}", service, msg->id());
}

std::uint8_t RemotePeer::next_empty_channel(const std::uint8_t id) {
	for(auto i = id; i < channels_.size(); ++i) {
		if(channels_[i].state() == Channel::State::EMPTY) {
			return i;
		}
	}

	return 0;
}

void RemotePeer::open_channel_response(core::Result result, std::uint8_t id, std::uint8_t requested) {
	core::OpenChannelResponseT response;
	response.actual_id = id;
	response.requested_id = requested;
	response.result = result;

	auto msg = std::make_unique<Message>();
	finish(response, *msg);
	write_header(*msg);
	conn_.send(std::move(msg));
}

void RemotePeer::handle_control_message(std::span<const std::uint8_t> data) {
	flatbuffers::Verifier verifier(data.data(), data.size());
	auto fb = core::GetHeader(data.data());
	
	if(!fb->Verify(verifier)) {
		LOG_WARN(log_) << "[spark] Bad Flatbuffer message" << LOG_ASYNC;
		return;
	}

	switch(fb->message_type()) {
		case core::Message::OpenChannel:
			handle_open_channel(fb->message_as_OpenChannel());
			break;
		case core::Message::CloseChannel:
			handle_close_channel(fb->message_as_CloseChannel());
			break;
		case core::Message::OpenChannelResponse:
			handle_open_channel_response(fb->message_as_OpenChannelResponse());
			break;
		case core::Message::Bye:
			//
			break;
		default:
			LOG_WARN(log_) << "[spark] Unknown control message type" << LOG_ASYNC;
	}
}

void RemotePeer::handle_close_channel(const core::CloseChannel* msg) {
	LOG_TRACE(log_) << __func__ << LOG_ASYNC;

	auto id = gsl::narrow<std::uint8_t>(msg->channel());
	Channel& channel = channels_[id];

	if(channel.state() == Channel::State::EMPTY) {
		LOG_WARN(log_) << "[spark] Request to close empty channel" << LOG_ASYNC;
		return;
	}

	channel.reset();
	LOG_INFO_FMT(log_, "[spark] Closed channel {}, requested by remote peer", id);
}

void RemotePeer::handle_channel_message(const MessageHeader& header,
                                        std::span<const std::uint8_t> data) {
	LOG_TRACE(log_) << __func__ << LOG_ASYNC;

	Channel& channel = channels_[header.channel];

	if(channel.state() != Channel::State::OPEN) {
		LOG_WARN_FMT(log_, "[spark] Received message for closed channel, {}", header.channel);
		return;
	}

	channel.message(header, data);
}

void RemotePeer::open_channel(const std::string& name, Handler* handler) {
	LOG_TRACE(log_) << __func__ << LOG_ASYNC;
	LOG_DEBUG_FMT(log_, "[spark] Requesting channel for {}", name);

	Channel& channel = channels_[next_channel_id_];
	channel.state(Channel::State::HALF_OPEN);
	channel.handler(handler);

	core::OpenChannelT body;
	body.id = next_channel_id_;
	body.service = name;
	++next_channel_id_ %= 256; // todo, need to properly handle IDs

	auto msg = std::make_unique<Message>();
	finish(body, *msg);
	write_header(*msg);
	conn_.send(std::move(msg));
}

void RemotePeer::start() {
	conn_.start();
}

} // v2, spark, ember