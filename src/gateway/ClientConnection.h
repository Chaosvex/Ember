/*
 * Copyright (c) 2016 - 2020 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ClientHandler.h"
#include "ConnectionStats.h"
#include "PacketCrypto.h"
#include "FilterTypes.h"
#include "packetlog/PacketLogger.h"
#include <logger/Logging.h>
#include <spark/buffers/DynamicBuffer.h>
#include <shared/ClientUUID.h>
#include <shared/memory/ASIOAllocator.h>
#include <botan/bigint.h>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <optional>
#include <mutex>
#include <string>
#include <utility>
#include <cstdint>
#include <cstddef>

namespace ember {

class SessionManager;

class ClientConnection final {
	static constexpr std::uint16_t INBOUND_SIZE { 1024 };
	static constexpr std::uint16_t OUTBOUND_SIZE { 2048 };

	enum class ReadState { HEADER, BODY, DONE } read_state_;

	boost::asio::ip::tcp::socket socket_;
	const boost::asio::ip::tcp::endpoint ep_;

	spark::DynamicBuffer<INBOUND_SIZE> inbound_buffer_;
	std::array<spark::DynamicBuffer<OUTBOUND_SIZE>, 2> outbound_buffers_;
	spark::DynamicBuffer<OUTBOUND_SIZE>* outbound_front_;
	spark::DynamicBuffer<OUTBOUND_SIZE>* outbound_back_;

	ClientHandler handler_;
	ConnectionStats stats_;
	std::optional<PacketCrypto> crypt_;
	protocol::SizeType msg_size_;
	SessionManager& sessions_;
	ASIOAllocator allocator_; // todo - should be shared & passed in
	log::Logger* logger_;
	bool write_in_progress_;
	unsigned int compression_level_;
	const std::string address_;
	std::unique_ptr<PacketLogger> packet_logger_;

	std::condition_variable stop_condvar_;
	std::mutex stop_lock_;
	std::atomic_bool stopped_;
	bool stopping_;

	// socket I/O
	void read();
	void write();

	// session management
	void stop();
	void close_session_sync();
	void terminate();

	// packet reassembly & dispatching
	void dispatch_message(spark::Buffer& buffer);
	void process_buffered_data(spark::Buffer& buffer);
	void parse_header(spark::Buffer& buffer);
	void completion_check(const spark::Buffer& buffer);

public:
	ClientConnection(SessionManager& sessions, boost::asio::ip::tcp::socket socket,
	                 boost::asio::ip::tcp::endpoint ep, ClientUUID uuid, log::Logger* logger)
	                 : sessions_(sessions), socket_(std::move(socket)), ep_(ep), stats_{},
	                   msg_size_{0}, logger_(logger), read_state_(ReadState::HEADER), stopped_(true),
	                   write_in_progress_(false),
	                   handler_(*this, uuid, logger, socket_.get_executor()), compression_level_(0),
	                   outbound_front_(&outbound_buffers_.front()),
	                   outbound_back_(&outbound_buffers_.back()), stopping_(false) { }

	void start();

	void set_key(const std::span<std::uint8_t>& key);
	void compression_level(unsigned int level);
	void latency(std::size_t latency);

	const ConnectionStats& stats() const;
	std::string remote_address() const;
	void log_packets(bool enable);

	template<typename PacketT> void send(const PacketT& packet);

	static void async_shutdown(std::shared_ptr<ClientConnection> client);
	void close_session(); // should be made private
};

#include "ClientConnection.inl"

} // ember