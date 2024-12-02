/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <spark/RemotePeer.h>
#include <spark/Common.h>
#include <spark/buffers/BufferAdaptor.h>
#include <spark/buffers/BinaryStream.h>
#include <spark/HandlerRegistry.h>
#include <spark/Peers.h>
#include <spark/Utility.h>
#include <shared/FilterTypes.h>
#include <gsl/gsl_util>

namespace ba = boost::asio;

namespace ember::spark {

RemotePeer::RemotePeer(boost::asio::io_context& ctx,
                       Connection connection, std::string banner,
                       std::string remote_banner, HandlerRegistry& registry,
                       log::Logger& log)
	: ctx_(ctx),
	  banner_(std::move(banner)),
	  remote_banner_(std::move(remote_banner)),
	  registry_(registry),
	  conn_(std::make_shared<Connection>(std::move(connection))),
	  log_(log) {
}

void RemotePeer::send(Message&& msg) {
	write_header(msg);
	conn_->send(std::move(msg));
}

void RemotePeer::receive(std::span<const std::uint8_t> data) {
	LOG_TRACE(log_) << log_func << LOG_ASYNC;

	spark::io::BufferAdaptor adaptor(data);
	spark::io::BinaryStream stream(adaptor);

	MessageHeader header;

	if(header.read_from_stream(stream) != MessageHeader::State::OK
	   || header.size <= stream.total_read()) {
		LOG_WARN_FILTER(log_, LF_SPARK)
			<< "[spark] Bad message from "
			<< conn_->address()
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

void RemotePeer::handle_open_channel_response(const core::OpenChannelResponse& msg) {
	LOG_TRACE(log_) << log_func << LOG_ASYNC;

	if(msg.result() != core::Result::OK) {
		auto& channel = channels_[msg.requested_id()];
		LOG_ERROR_ASYNC(log_, "[spark] Remote peer could not open channel ({}:{})",
		                channel->handler()->type(), msg.requested_id());
		channels_[msg.requested_id()].reset();
		return;
	}

	// todo, index bounds checks
	auto id = msg.actual_id();
	auto& channel = channels_[id];

	if(id == 0) {
		LOG_ERROR_ASYNC(log_, "[spark] Reserved channel ID returned by {}", remote_banner_);
		channels_[msg.requested_id()].reset();
		return;
	}

	if(msg.actual_id() != msg.requested_id()) {
		if(channel) {
			LOG_ERROR_ASYNC(log_, "[spark] Channel open ({}) failed due to ID collision",
			                msg.actual_id());
			send_close_channel(msg.actual_id());
			channels_[msg.requested_id()].reset();
			return;
		}

		channels_[msg.actual_id()] = channels_[msg.requested_id()];
		channels_[msg.requested_id()].reset();
		channel = channels_[id];
	}

	if(!channel || channel->is_open()) {
		send_close_channel(msg.actual_id());
		channels_[msg.actual_id()].reset();
		return;
	}

	channel->open();

	LOG_DEBUG_ASYNC(log_, "[spark] Remote channel open, {}:{}",
	                channel->handler()->name(), msg.actual_id());
}

void RemotePeer::send_close_channel(const std::uint8_t id) {
	LOG_TRACE(log_) << log_func << LOG_ASYNC;

	core::CloseChannelT body {
		.channel = id
	};

	Message msg;
	finish(body, msg);
	write_header(msg);
	conn_->send(std::move(msg));
}

Handler* RemotePeer::find_handler(const core::OpenChannel& msg) {
	const auto sname = msg.service_name();
	const auto stype = msg.service_type();

	if(sname && stype) {
		return registry_.service(sname->str(), stype->str());
	}

	if(sname) {
		return registry_.service(sname->str());
	}

	if(stype) {
		auto services = registry_.services(stype->str());

		// just use the first matching handler
		if(!services.empty()) {
			return services.front();
		}
	}

	return nullptr;
}

void RemotePeer::handle_open_channel(const core::OpenChannel& msg) {
	LOG_TRACE(log_) << log_func << LOG_ASYNC;

	auto handler = find_handler(msg);

	if(!handler) {
		LOG_DEBUG_ASYNC(log_, "[spark] Requested service handler ({}) does not exist",
		                msg.service_type()->str());
		open_channel_response(core::Result::ERROR_UNK, 0, msg.id());
		return;
	}

	if(msg.id() == 0 || msg.id() >= channels_.size()) {
		LOG_DEBUG_ASYNC(log_, "[spark] Bad channel ID ({}) specified", msg.id());
		open_channel_response(core::Result::ERROR_UNK, 0, msg.id());
		return;
	}

	auto id = gsl::narrow<std::uint8_t>(msg.id());

	if(channels_[id]) {
		if(id = next_empty_channel(); id == 0) {
			LOG_ERROR_ASYNC(log_, "[spark] Exhausted channel IDs");
			open_channel_response(core::Result::ERROR_UNK, 0, msg.id());
			return;
		}
	}

	auto channel = std::make_shared<Channel>(
		ctx_, id, remote_banner_, handler->name(), handler, conn_, log_
	);

	channel->open();
	channels_[id] = std::move(channel);
	open_channel_response(core::Result::OK, id, msg.id());
	LOG_DEBUG_ASYNC(log_, "[spark] Remote channel open, {}:{}", handler->name(), id);
}

std::uint8_t RemotePeer::next_empty_channel() {
	// zero is reserved
	for(auto i = 1; i < channels_.size(); ++i) {
		if(!channels_[i]) {
			return i;
		}
	}

	return 0;
}

void RemotePeer::open_channel_response(const core::Result result,
                                       const std::uint8_t id,
                                       const std::uint8_t requested) {
	const std::string& sname = channels_[id]->handler()->name();

	core::OpenChannelResponseT response {
		.result = result,
		.requested_id = requested,
		.actual_id = id,
		.service_name = sname,
		.banner = banner_,
	};

	Message msg;
	finish(response, msg);
	write_header(msg);
	conn_->send(std::move(msg));
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
			handle_open_channel(*fb->message_as_OpenChannel());
			break;
		case core::Message::CloseChannel:
			handle_close_channel(*fb->message_as_CloseChannel());
			break;
		case core::Message::OpenChannelResponse:
			handle_open_channel_response(*fb->message_as_OpenChannelResponse());
			break;
		case core::Message::Ping:
			handle_ping(*fb->message_as_Ping());
			break;
		case core::Message::Pong:
			handle_pong(*fb->message_as_Pong());
			break;
		default:
			LOG_WARN(log_) << "[spark] Unknown control message type" << LOG_ASYNC;
	}
}

void RemotePeer::handle_ping(const core::Ping& ping) {
	core::PongT pong {
		.sequence = ping.sequence()
	};

	Message msg;
	finish(pong, msg);
	write_header(msg);
	conn_->send(std::move(msg));
}

void RemotePeer::handle_pong(const core::Pong& pong) {
	if(pong.sequence() != ping_sequence_) {
		LOG_DEBUG(log_) << "[spark] Bad pong sequence" << LOG_ASYNC;
		return;
	}

	const auto delta = std::chrono::steady_clock::now() - ping_time_;
	
	if(delta > LATENCY_WARN_THRESHOLD) {
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(delta);
		LOG_WARN_ASYNC(log_, "[spark] Remote peer is slow to respond {} ({})", ms, remote_banner_);
	}
}

void RemotePeer::handle_close_channel(const core::CloseChannel& msg) {
	LOG_TRACE(log_) << log_func << LOG_ASYNC;

	auto id = gsl::narrow<std::uint8_t>(msg.channel());

	if(!channels_[id]) {
		LOG_WARN_ASYNC(log_, "[spark] Request to close empty channel ({})", id);
		return;
	}

	channels_[id]->close();
	channels_[id].reset();
	LOG_DEBUG_ASYNC(log_, "[spark] Closed channel ({}), requested by remote peer", id);
}

void RemotePeer::handle_channel_message(const MessageHeader& header,
                                        std::span<const std::uint8_t> data) {
	LOG_TRACE(log_) << log_func << LOG_ASYNC;

	auto& channel = channels_[header.channel];

	if(!channel || !channel->is_open()) {
		LOG_WARN_ASYNC(log_, "[spark] Received message for closed channel ({})", header.channel);
		return;
	}

	channel->dispatch(header, data);
}

void RemotePeer::send_open_channel(std::string name, std::string type, const std::uint8_t id) {
	core::OpenChannelT body {
		.id = id,
		.service_type = std::move(type),
		.service_name = std::move(name)
	};

	Message msg;
	finish(body, msg);
	write_header(msg);
	conn_->send(std::move(msg));
}

void RemotePeer::open_channel(std::string type, gsl::not_null<Handler*> handler) {
	LOG_TRACE(log_) << log_func << LOG_ASYNC;

	const auto id = next_empty_channel();
	LOG_DEBUG_ASYNC(log_, "[spark] Requesting channel {} for {}", id, type);

	auto channel = std::make_shared<Channel>(
		ctx_, id, remote_banner_, handler->name(), handler, conn_, log_
	);

	channels_[id] = std::move(channel);
	send_open_channel("", std::move(type), id);
}

void RemotePeer::start() {
	conn_->start([this](std::span<const std::uint8_t> data) {
		receive(data);
	});
}

// very temporary, not thread-safe etc
void RemotePeer::remove_handler(gsl::not_null<Handler*> handler) {
	for(std::size_t i = 0u; i < channels_.size(); ++i) {
		auto& channel = channels_[i];

		if(!channel) {
			continue;
		}

		if(channel->handler() == handler) {
			send_close_channel(i);
			channels_[i].reset();
		}
	}
}

RemotePeer::~RemotePeer() {
	for(auto& channel : channels_) {
		if(channel) {
			channel->close();
		}
	}
}

} // spark, ember