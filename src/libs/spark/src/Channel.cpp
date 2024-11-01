/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <spark/Channel.h>
#include <spark/Connection.h>
#include <spark/Common.h>
#include <spark/buffers/BinaryStream.h>
#include <spark/buffers/BufferAdaptor.h>
#include <cassert>

namespace ember::spark {

Channel::Channel(boost::asio::io_context& ctx, std::uint8_t id,
                 std::string banner, std::string service,
                 Handler* handler, std::shared_ptr<Connection> connection,
                 log::Logger& logger)
	: tracking_(ctx, logger),
	  channel_id_(id),
	  handler_(handler),
      connection_(std::move(connection)),
	  link_ { .peer_banner = std::move(banner), .service_name = std::move(service) } {}

void Channel::open() {
	if(state_ != State::OPEN) {
		state_ = State::OPEN;
		link_up();
	}
}

bool Channel::is_open() const {
	return state_ == State::OPEN;
}

void Channel::dispatch(const MessageHeader& header, std::span<const std::uint8_t> data) {
	link_.channel = weak_from_this();

	if(!header.response || header.uuid.is_nil()) {
		handler_->on_message(link_, data, header.uuid);
	} else { // tracked message response
		tracking_.on_message(link_, data, header.uuid);
	}
}

bool Channel::send(flatbuffers::FlatBufferBuilder&& fbb, const Token& token, const bool response) {
	Message msg;
	msg.fbb = std::move(fbb);

	MessageHeader header;
	header.uuid = token;
	header.response = response;
	header.channel = channel_id_;
	header.size = msg.fbb.GetSize();
	header.set_alignment(msg.fbb.GetBufferMinAlignment());

	io::BufferAdaptor adaptor(msg.header);
	io::BinaryStream stream(adaptor);
	header.write_to_stream(stream);
	connection_->send(std::move(msg));
	return true;
}

bool Channel::send(flatbuffers::FlatBufferBuilder&& fbb, TrackedState state,
                   std::chrono::seconds timeout) {
	if(!is_open()) {
		state(link_, std::unexpected(Result::CHANNEL_CLOSED));
		return false;
	}

	const auto token = uuid_gen_();
	tracking_.track(token, state, timeout);
	send(std::move(fbb), token, false);
	return true;
}

bool Channel::send(flatbuffers::FlatBufferBuilder&& fbb) {
	if(!is_open()) {
		return false;
	}

	return send(std::move(fbb), {}, false);
}

bool Channel::send(flatbuffers::FlatBufferBuilder&& fbb, const Token& token) {
	if(!is_open()) {
		return false;
	}

	return send(std::move(fbb), token, true);
}

auto Channel::state() const -> State {
	return state_;
}

Handler* Channel::handler() const {
	return handler_;
}

void Channel::link_up() {
	assert(handler_);
	link_.channel = weak_from_this();
	handler_->on_link_up(link_);
}

void Channel::close() {
	if(handler_ && is_open()) {
		link_.channel = weak_from_this();
		tracking_.shutdown();
		handler_->on_link_down(link_);
	}

	state_ = State::CLOSED;
}

Channel::~Channel() {
	close();
}

} // spark, ember