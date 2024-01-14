/*
 * Copyright (c) 2023 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stun/Client.h>
#include <stun/Protocol.h>
#include <stun/DatagramTransport.h>
#include <stun/StreamTransport.h>
#include <spark/buffers/BinaryStream.h>
#include <spark/buffers/VectorBufferAdaptor.h>
#include <shared/util/FNVHash.h>
#include <boost/assert.hpp>
#include <boost/asio.hpp>
#include <boost/endian.hpp>
#include <stdexcept>
#include <utility>
#include <vector>
#include <cstddef>

#include <iostream> // todo temp

namespace ember::stun {

Client::Client(RFCMode mode) : mode_(mode), mt_(rd_()) {
	work_.emplace_back(std::make_shared<boost::asio::io_context::work>(ctx_));
	worker_ = std::jthread(static_cast<size_t(boost::asio::io_context::*)()>
		(&boost::asio::io_context::run), &ctx_);
}

Client::~Client() {
	work_.clear();
	transport_.reset();
	ctx_.stop();
}

void Client::log_callback(LogCB callback, const Verbosity verbosity) {
	if(!callback) {
		throw std::invalid_argument("Logging callback cannot be null");
	}

	logger_ = callback;
	verbosity_ = verbosity;
}

void Client::connect(const std::string& host, const std::uint16_t port, const Protocol protocol) {
	transport_.reset();

	switch(protocol) {
	case Protocol::UDP:
		transport_ = std::make_unique<DatagramTransport>(ctx_, host, port,
			[this](std::vector<std::uint8_t> buffer) { handle_response(std::move(buffer)); });
		break;
	case Protocol::TCP:
		transport_ = std::make_unique<StreamTransport>(host, port);
		break;
	case Protocol::TLS_TCP:
		throw std::runtime_error("TLS_TCP STUN isn't handled yet");
	default:
		throw std::invalid_argument("Unknown protocol value specified");
	}

	transport_->connect();
}

void Client::handle_response(std::vector<std::uint8_t> buffer) try {
	if(buffer.size() < HEADER_LENGTH) {
		logger_(Verbosity::STUN_LOG_DEBUG, LogReason::RESP_BUFFER_LT_HEADER);
		return; // RFC says invalid messages should be discarded
	}

	spark::VectorBufferAdaptor<std::uint8_t> vba(buffer);
	spark::BinaryInStream stream(vba);

	Header header{};
	stream >> header.type;
	stream >> header.length;
	stream >> header.cookie;

	if(mode_ == RFCMode::RFC5389) {
		stream.get(header.tx_id_5389.begin(), header.tx_id_5389.end());
	} else {
		stream.get(header.tx_id_3489.begin(), header.tx_id_3489.end());
	}

	if(mode_ == RFCMode::RFC5389 && header.cookie != MAGIC_COOKIE) {
		logger_(Verbosity::STUN_LOG_DEBUG, LogReason::RESP_COOKIE_MISSING);
		return;
	}

	if(header.length < ATTR_HEADER_LENGTH) {
		logger_(Verbosity::STUN_LOG_DEBUG, LogReason::RESP_BAD_HEADER_LENGTH);
		return;
	}

	// Check to see whether this is a response that we're expecting
	const auto hash = header_hash(header);

	if(!transactions_.contains(hash)) {
		logger_(Verbosity::STUN_LOG_DEBUG, LogReason::RESP_TX_NOT_FOUND);
		return;
	}

	Transaction& transaction = transactions_[hash];

	MessageType type{ static_cast<std::uint16_t>(header.type) };

	if (type == MessageType::BINDING_ERROR_RESPONSE) {
		handle_error_response(stream);
	} else {
		handle_attributes(stream, transaction);
	}
} catch(const std::exception& e) {
	std::cout << e.what(); // temp
}

void Client::handle_error_response(spark::BinaryInStream& stream) {

}

void Client::handle_attributes(spark::BinaryInStream& stream, Transaction& tx) {
	Attributes attribute;
	be::big_uint16_t length;
	stream >> attribute;
	stream >> length;

	be::big_to_native_inplace(attribute);

	switch(attribute) {
		case Attributes::MAPPED_ADDRESS:
			handle_mapped_address(stream, length);
			break;
		case Attributes::XOR_MAPPED_ADDRESS:
			handle_xor_mapped_address(stream, length);
			break;
	}

	tx.promise.set_value("Hello, world!"); // temp
}

void Client::xor_buffer(std::span<std::uint8_t> buffer, const std::vector<std::uint8_t>& key) {
	for (std::size_t i = 0u; i < buffer.size();) {
		for (std::size_t j = 0u; j < key.size(); ++j) {
			buffer[i] ^= key.data()[j];
			++i;
		}
	}
}

void Client::handle_mapped_address(spark::BinaryInStream& stream, const std::uint16_t length) {
	// shouldn't receive this attribute in RFC5389 mode but we'll allow it
	if (mode_ == RFCMode::RFC5389) {
		logger_(Verbosity::STUN_LOG_DEBUG, LogReason::RESP_RFC5389_INVALID_ATTRIBUTE);
	}

	stream.skip(1); // skip reserved byte
	AddressFamily addr_fam = AddressFamily::IPV4;
	stream >> addr_fam;

	// Only IPv4 is supported prior to RFC5389
	if(addr_fam != AddressFamily::IPV4) {
		logger_(Verbosity::STUN_LOG_DEBUG, LogReason::RESP_IPV6_NOT_VALID);
		return;
	}

	std::uint16_t port = 0;
	stream >> port;
	be::big_to_native_inplace(port);

	std::uint32_t ipv4 = 0;
	stream >> ipv4;
	be::big_to_native_inplace(ipv4);

	boost::asio::ip::address_v4 addr(ipv4);
	std::cout << addr.to_string() << ":" << port;
}

void Client::handle_xor_mapped_address(spark::BinaryInStream& stream, const std::uint16_t length) {
	// shouldn't receive this attribute in RFC3489 mode but we'll allow it
	if(mode_ == RFCMode::RFC3489) {
		logger_(Verbosity::STUN_LOG_DEBUG, LogReason::RESP_RFC3489_INVALID_ATTRIBUTE);
	}

	stream.skip(1); // skip reserved byte
	AddressFamily addr_fam = AddressFamily::IPV4;
	stream >> addr_fam;

	// XOR port with the magic cookie
	std::uint16_t port = 0;
	stream >> port;
	auto magic = MAGIC_COOKIE;
	be::native_to_big_inplace(magic);
	port ^= magic;
	be::big_to_native_inplace(port);

	if(addr_fam == AddressFamily::IPV4) {
		std::uint32_t ipv4 = 0;
		stream >> ipv4;
		be::big_to_native_inplace(ipv4);
		ipv4 ^= MAGIC_COOKIE;
		boost::asio::ip::address_v4 addr(ipv4);
		std::cout << addr.to_string();
	} else if(addr_fam == AddressFamily::IPV6) {
		/*
		spark::VectorBufferAdaptor<std::uint8_t> vba(xor_data);
		spark::BinaryInStream xor_stream(vba);
		std::uint64_t ipv6 = 0;
		xor_stream >> ipv6;
		be::big_to_native_inplace(ipv6);*/
	} else {
		// todo
	}
}

std::future<std::string> Client::mapped_address() {
	std::vector<std::uint8_t> data;
	spark::VectorBufferAdaptor buffer(data);
	spark::BinaryOutStream stream(buffer);

	Header header { };
	header.type = std::to_underlying(MessageType::BINDING_REQUEST);
	header.length = 0;

	if(mode_ == RFCMode::RFC5389) {
		header.cookie = MAGIC_COOKIE;

		for(auto& ele : header.tx_id_5389) {
			ele = mt_();
		}
	} else {
		for(auto& ele : header.tx_id_3489) {
			ele = mt_();
		}
	}

	stream << header.type;
	stream << header.length;

	if(mode_ == RFCMode::RFC5389) {
		stream << header.cookie;
		stream.put(header.tx_id_5389.begin(), header.tx_id_5389.end());
	} else {
		stream.put(header.tx_id_3489.begin(), header.tx_id_3489.end());
	}

	Transaction transaction{};

	if (mode_ == RFCMode::RFC5389) {
		std::copy(header.tx_id_5389.begin(), header.tx_id_5389.end(), transaction.tx_id);
	} else {
		std::copy(header.tx_id_3489.begin(), header.tx_id_3489.end(), transaction.tx_id);
	}

	transaction.promise = std::promise<std::string>();
	auto future = transaction.promise.get_future();
	const auto hash = header_hash(header);
	transactions_[hash] = std::move(transaction);

	transport_->send(data);
	return future;
}

std::size_t Client::header_hash(const Header& header) {
	/*
	 * Hash the transaction ID to use as a key for future lookup.
	 * FNV is used because it's already in the project, not for any
	 * particular property. Odds of a collision are very low. 
	 */
	FNVHash fnv;

	if(mode_ == RFCMode::RFC5389) {
		fnv.update(header.tx_id_5389.begin(), header.tx_id_5389.end());
	} else {
		fnv.update(header.tx_id_3489.begin(), header.tx_id_3489.end());
	}

	return fnv.hash();
}

void Client::software() {
	//std::vector<std::uint8_t> data;
	//spark::VectorBufferAdaptor buffer(data);
	//spark::BinaryOutStream stream(buffer);

	//Header header{ };
	//header.type = (uint16_t)Attributes::SOFTWARE;
	//header.length = 0;
	//header.trans_id_5389[0] = 5;

	//if (mode_ == RFCMode::RFC5389) {
	//	header.cookie = MAGIC_COOKIE;
	//}
	//else {
	//	header.cookie = 0;
	//}

	//stream << header;
	//transport_->send(data);
}

} // stun, ember