/*
 * Copyright (c) 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <spark/v2/Handler.h>
#include <spark/v2/MessageHeader.h>
#include <spark/v2/Link.h>
#include <spark/v2/Tracking.h>
#include <logger/Logger.h>
#include <boost/asio/io_context.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/functional/hash.hpp>
#include <flatbuffers/flatbuffer_builder.h>
#include <functional>
#include <memory>
#include <span>
#include <cstdint>

namespace ember::spark::v2 {

class Connection;

using namespace std::chrono_literals;

class Channel final : public std::enable_shared_from_this<Channel> {
public:
	enum class State {
		AWAITING, OPEN, CLOSED
	};

private:
	Tracking tracking_;
	State state_ = State::AWAITING;
	std::uint8_t channel_id_;
	Handler* handler_;
	std::shared_ptr<Connection> connection_;
	Link link_;
	boost::uuids::random_generator uuid_gen_;

	void link_up();
	void send(flatbuffers::FlatBufferBuilder&& fbb, const Token& token, const bool response);

public:
	Channel(boost::asio::io_context& ctx, std::uint8_t id,
	        std::string banner, std::string service, 
	        Handler* handler, std::shared_ptr<Connection> connection,
	        log::Logger* logger);

	Channel() = default;
	~Channel();

	Handler* handler() const;
	State state() const;
	bool is_open() const;

	void open();
	void close();
	void dispatch(const MessageHeader& header, std::span<const std::uint8_t> data);
	void send(flatbuffers::FlatBufferBuilder&& fbb, TrackedState state,
	          std::chrono::seconds timeout = 5s);
	void send(flatbuffers::FlatBufferBuilder&& fbb);
	void send(flatbuffers::FlatBufferBuilder&& fbb, const Token& token);
};

} // v2, spark, ember