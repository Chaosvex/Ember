/*
 * Copyright (c) 2023 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stun/Client.h>
#include <stun/DatagramTransport.h>
#include <stun/StreamTransport.h>
#include <stun/Exception.h>
#include <stun/Parser.h>
#include <stun/MessageBuilder.h>
#include <stun/Utility.h>
#include <stun/detail/Shared.h>
#include <shared/threading/Utility.h>
#include <boost/assert.hpp>
#include <boost/endian.hpp>
#include <chrono>
#include <stdexcept>
#include <utility>

using namespace std::chrono_literals;

template<class>
inline constexpr bool always_false_v = false;

namespace ember::stun {

using namespace detail;

Client::Client(const std::string& bind, std::string host, const std::uint16_t port,
               Protocol proto, RFCMode mode)
	: proto_(proto), mode_(mode), host_(std::move(host)), port_(port) {

	// worker used by timers
	work_ = std::make_unique<boost::asio::io_context::work>(ctx_);
	worker_ = std::jthread(static_cast<size_t(boost::asio::io_context::*)()>
						   (&boost::asio::io_context::run), &ctx_);
	thread::set_name(worker_, "STUN Worker");

	switch(proto) {
		case Protocol::TCP:
			transport_ = std::make_unique<StreamTransport>(bind);
			break;
		case Protocol::UDP:
			transport_ = std::make_unique<DatagramTransport>(bind);
			break;
	}

	transport_->set_callbacks(
		[this](std::vector<std::uint8_t> buffer) { handle_message(buffer); },
		[this](const boost::system::error_code& ec) { on_connection_error(ec); }
	);
}

void Client::create_transaction(Transaction::Promise promise) {
	tx_ = std::make_unique<Transaction>(
		ctx_.get_executor(), transport_->timeout(), transport_->retries()
	);

	tx_->promise = std::move(promise);
}

Client::~Client() {
	work_.reset();
	transport_.reset();
}

void Client::log_callback(LogCB callback) {
	if(!callback) {
		throw exception(Error::BAD_CALLBACK, "Logging callback cannot be null");
	}

	logger_ = callback;
}

void Client::connect(const std::string& host, const std::uint16_t port,
                     Transport::OnConnect&& cb) {
	dest_hist_.insert_or_assign(host, std::chrono::steady_clock::now());

	// wrap the callback to save error checking and locking duplication
	transport_->connect(host, port, 
		[&, cb = std::move(cb)](const boost::system::error_code& ec) {
			std::lock_guard guard(mutex_);

			if(!ec) {
				cb(ec);
			} else {
				abort_transaction(Error::UNABLE_TO_CONNECT);
			}
		}
	);
}

std::pair<std::shared_ptr<std::vector<std::uint8_t>>, std::size_t>
Client::build_request(const bool change_ip, const bool change_port) {
	MessageBuilder builder(MessageType::BINDING_REQUEST, mode_);

	if(!(opts_ & SUPPRESS_BANNER)) {
		builder.add_software(SOFTWARE_DESC);
	}

	if(change_ip || change_port) {
		builder.add_change_request(change_ip, change_port);
	}

	auto buffer = std::make_shared<std::vector<std::uint8_t>>(builder.final(true));
	return { buffer, builder.key() };
}

void Client::handle_message(std::span<const std::uint8_t> buffer) try {
	std::lock_guard guard(mutex_);

	if(buffer.size() < HEADER_LENGTH) {
		logger_(Verbosity::STUN_LOG_DEBUG, Error::RESP_BUFFER_LT_HEADER);
		return; // RFC says invalid messages should be discarded
	}

	Parser parser(buffer, mode_);
	parser.set_logger(logger_);

	const Header& header = parser.read_header();

	// Check to see whether this is a response that we're expecting
	const auto hash = generate_key(header.tx_id, mode_);

	if(tx_ && tx_->key != hash) {
		logger_(Verbosity::STUN_LOG_DEBUG, Error::RESP_TX_NOT_FOUND);
		return;
	}

	tx_->timer.cancel();

	if(auto error = parser.validate_header(header); error != Error::OK) {
		abort_transaction(error);
		return;
	}

	process_message(buffer);
} catch(const spark::exception& e) {
	logger_(Verbosity::STUN_LOG_DEBUG, Error::BUFFER_PARSE_ERROR);
}

void Client::process_message(std::span<const std::uint8_t> buffer) try {
	Parser parser(buffer, mode_);
	parser.set_logger(logger_);

	const auto header = parser.read_header();
	auto attributes = parser.attributes();

	if(const auto fp = retrieve_attribute<attributes::Fingerprint>(attributes)) {
		const auto crc32 = parser.fingerprint();

		if(crc32 != fp->crc32) {
			abort_transaction(Error::RESP_INVALID_FINGERPRINT);
			return;
		}
	}

	tx_->attributes = std::move(attributes);

	const MessageType type{ header.type.value() };

	switch(type) {
		case MessageType::BINDING_RESPONSE:
			handle_binding_resp();
			break;
		case MessageType::BINDING_ERROR_RESPONSE:
			handle_binding_err_resp();
			break;
		case MessageType::BINDING_REQUEST:
			handle_binding_req();
			break;
		default:
			abort_transaction(Error::RESP_UNK_MESSAGE_TYPE);
	}
} catch (const spark::exception&) {
	abort_transaction(Error::BUFFER_PARSE_ERROR);
}

void Client::abort_transaction(const Error error, attributes::ErrorCode ec, const bool erase) {
	if(!tx_) {
		return;
	}

	std::visit([&](auto&& arg) {
		arg.set_value(std::unexpected(ErrorRet(error, ec)));
	}, tx_->promise);

	tx_.reset();
}

void Client::set_nat_present() {
	attributes::MappedAddress mapped{};
	const auto xma = retrieve_attribute<attributes::XorMappedAddress>(tx_->attributes);

	// we don't care if it's a mapped or xormapped attribute, as long we have one
	if(xma) {
		mapped.family = xma->family;
		mapped.ipv4 = xma->ipv4;
		mapped.ipv6 = xma->ipv6;
		mapped.port = xma->port;
	} else {
		const auto ma = retrieve_attribute<attributes::MappedAddress>(tx_->attributes);

		if(!ma) {
			return;
		}

		mapped = *ma;
	}
	
	const std::string& bind_ip = extract_ip_to_string(mapped);
	is_nat_present_ = (transport_->local_ip() != bind_ip);
}

void Client::handle_binding_req() {
	if(tx_->state == State::HAIRPIN_AWAIT_RESP) {
		tx_->data.hairpinning_result = Hairpinning::SUPPORTED;
		complete_transaction();
	} else {
		abort_transaction(Error::RESP_UNHANDLED_RESP_TYPE);
	}
}

void Client::perform_mapping_test3() {
	const auto xma_attr = retrieve_attribute<attributes::XorMappedAddress>(tx_->attributes);

	if(!xma_attr) {
		abort_transaction(Error::RESP_MISSING_ATTR);
		return;
	}

	if(*xma_attr != tx_->data.xmapped) {
		const auto alternate_address = extract_ip_to_string(tx_->data.otheradd);

		connect(alternate_address, port_, [&](const boost::system::error_code& ec) {
			create_transaction(std::move(tx_->promise));
			auto [buffer, key] = build_request();
			rearm_transaction(State::MAPPING_TEST_3, key, buffer);
			start_transaction_timer();
			transport_->send(buffer);
		});
	} else {
		tx_->data.behaviour_result = Behaviour::ENDPOINT_INDEPENDENT;
		complete_transaction();
	}
}

void Client::rearm_transaction(State state, std::size_t key,
                               std::shared_ptr<std::vector<std::uint8_t>> buffer,
                               Transaction::TestData data) {
	tx_->state = state;
	tx_->key = key;
	tx_->retry_buffer = std::move(buffer);
	tx_->data = std::move(data);
}

void Client::perform_filtering_test2() {
	const auto oa_attr = retrieve_attribute<attributes::OtherAddress>(tx_->attributes);

	if(!oa_attr) {
		abort_transaction(Error::UNSUPPORTED_BY_SERVER);
		return;
	}

	create_transaction(std::move(tx_->promise));
	auto [buffer, key] = build_request(true, false);
	rearm_transaction(State::FILTERING_TEST_2, key, buffer);
	start_transaction_timer();
	transport_->send(buffer);
}

void Client::perform_filtering_test3() {
	create_transaction(std::move(tx_->promise));
	auto [buffer, key] = build_request(false, true);
	rearm_transaction(State::FILTERING_TEST_3, key, buffer);
	start_transaction_timer();
	transport_->send(buffer);
}

void Client::mapping_test_result() {
	const auto xma_attr = retrieve_attribute<attributes::XorMappedAddress>(tx_->attributes);

	if(!xma_attr) {
		abort_transaction(Error::RESP_MISSING_ATTR);
		return;
	}

	if(tx_->data.xmapped == xma_attr) {
		tx_->data.behaviour_result = Behaviour::ADDRESS_DEPENDENT;
		complete_transaction();
	} else {
		tx_->data.behaviour_result = Behaviour::ADDRESS_PORT_DEPENDENT;
		complete_transaction();
	}
}

void Client::perform_hairpinning_test() {
	const auto xma_attr = retrieve_attribute<attributes::XorMappedAddress>(tx_->attributes);

	if(!xma_attr) {
		abort_transaction(Error::RESP_MISSING_ATTR);
		return;
	}

	const auto bind_ip = extract_ip_to_string(*xma_attr);

	transport_->connect(bind_ip, xma_attr->port, [&](const boost::system::error_code&) {
		create_transaction(std::move(tx_->promise));
		auto [buffer, key] = build_request();
		rearm_transaction(State::HAIRPIN_AWAIT_RESP, key, buffer);
		start_transaction_timer();
		transport_->send(buffer);
	});
}

void Client::perform_mapping_test2() {
	const auto oa_attr = retrieve_attribute<attributes::OtherAddress>(tx_->attributes);
	const auto xma_attr = retrieve_attribute<attributes::XorMappedAddress>(tx_->attributes);

	if(!oa_attr || !xma_attr) {
		abort_transaction(Error::UNSUPPORTED_BY_SERVER);
		return;
	}

	const auto bind_ip = extract_ip_to_string(*xma_attr);

	auto& data = tx_->data;
	data.otheradd = *oa_attr;
	data.xmapped = *xma_attr;

	if(bind_ip == transport_->local_ip()
		&& xma_attr->port == transport_->local_port()) {
		data.behaviour_result = Behaviour::ENDPOINT_INDEPENDENT;
		complete_transaction();
	} else {
		auto alternate_address = extract_ip_to_string(*oa_attr);
			
		connect(alternate_address, port_, [&](const boost::system::error_code&) {
			auto data = tx_->data;
			create_transaction(std::move(tx_->promise));
			auto [buffer, key] = build_request();
			rearm_transaction(State::MAPPING_TEST_2, key, buffer, data);
			start_transaction_timer();
			transport_->send(buffer);
		});
	}
}

void Client::handle_binding_resp() {
	set_nat_present();
	
	switch(tx_->state) {
		case State::BASIC:
			complete_transaction();
			break;
		case State::MAPPING_TEST_1:
			perform_mapping_test2();
			break;
		case State::MAPPING_TEST_2:
			perform_mapping_test3();
			break;
		case State::MAPPING_TEST_3:
			mapping_test_result();
			break;
		case State::HAIRPIN:
			perform_hairpinning_test();
			break;
		case State::FILTERING_TEST_1:
			perform_filtering_test2();
			break;
		case State::FILTERING_TEST_2:
			tx_->data.behaviour_result = Behaviour::ENDPOINT_INDEPENDENT;
			complete_transaction();
			break;
		case State::FILTERING_TEST_3:
			tx_->data.behaviour_result = Behaviour::ADDRESS_DEPENDENT;
			complete_transaction();
			break;
		default:
			abort_transaction(Error::CONNECTION_ERROR);
	}
}

void Client::handle_binding_err_resp() {
	const auto& ec = retrieve_attribute<attributes::ErrorCode>(tx_->attributes);

	if(!ec) {
		abort_transaction(Error::RESP_MISSING_ATTR);
		return;
	}

	if(Errors(ec->code) == Errors::TRY_ALTERNATE) {
		// insurance policy against being bounced around servers
		if(tx_->redirects >= MAX_REDIRECTS) {
			abort_transaction(Error::RESP_BAD_REDIRECT);
			return;
		}

		++tx_->redirects;

		const auto alt_attr = retrieve_attribute<attributes::AlternateServer>(tx_->attributes);

		if(!alt_attr) {
			abort_transaction(Error::RESP_MISSING_ATTR);
			return;
		}

		/*
		 * RFC states "The IP address family MUST be identical
		 * to that of the source IP address of the request".
		 * I don't have a reliable way of doing this, so going to ignore it.
		 */
		const std::string& ip = extract_ip_to_string(*alt_attr);

		// check we haven't resent to this address previously to prevent a redirect loop
		if(auto entry = dest_hist_.find(ip); entry != dest_hist_.end()) {
			if(std::chrono::steady_clock::now() - entry->second < 5min) {
				abort_transaction(Error::RESP_BAD_REDIRECT);
				return;
			}
		}

		// retry the request
		transport_->connect(ip, alt_attr->port, [&](const boost::system::error_code&) {
			auto redirects = tx_->redirects;
			auto state = tx_->state;
			create_transaction(std::move(tx_->promise));
			auto [buffer, key] = build_request();
			rearm_transaction(state, key, buffer);
			tx_->redirects = redirects;
			start_transaction_timer();
			transport_->send(buffer);
		});
	} else {
		if(ec) {
			abort_transaction(Error::RESP_BINDING_ERROR, *ec);
		} else {
			abort_transaction(Error::RESP_BINDING_ERROR);
		}
	}
}

void Client::complete_transaction() {
	std::visit([&](auto&& arg) {
		using T = std::decay_t<decltype(arg)>;

		if constexpr(std::is_same_v<T, std::promise<MappedResult>>) {
			auto mapped = retrieve_attribute<attributes::MappedAddress>(tx_->attributes);
			
			if(mapped) {
				arg.set_value(*mapped);
				return;
			}

			// XorMappedAddress will also do - we just need an external address
			auto xmapped = retrieve_attribute<attributes::XorMappedAddress>(tx_->attributes);

			if(!xmapped) {
				arg.set_value(std::unexpected(ErrorRet(Error::RESP_MISSING_ATTR)));
				return;
			}

			const attributes::MappedAddress ma{
				.ipv4 = xmapped->ipv4,
				.ipv6 = xmapped->ipv6,
				.port = xmapped->port,
				.family = xmapped->family
			};

			arg.set_value(ma);
		} else if constexpr(std::is_same_v<T, std::promise<AttributesResult>>) {
			arg.set_value(std::move(tx_->attributes));
		} else if constexpr(std::is_same_v<T, std::promise<BehaviourResult>>) {
			arg.set_value(tx_->data.behaviour_result);
		} else if constexpr(std::is_same_v<T, std::promise<HairpinResult>>) {
			arg.set_value(tx_->data.hairpinning_result);
		} else if constexpr(std::is_same_v<T, std::promise<NATResult>>) {
			if(is_nat_present_) {
				arg.set_value(*is_nat_present_);
			} else {
				arg.set_value(std::unexpected(ErrorRet(Error::RESP_MISSING_ATTR)));
			}
		} else {
			static_assert(always_false_v<T>, "Unhandled variant type");
		}
	}, tx_->promise);

	tx_.reset();
}

void Client::handle_no_response() {
	switch(tx_->state) {
		case State::HAIRPIN_AWAIT_RESP:
			tx_->data.hairpinning_result = Hairpinning::NOT_SUPPORTED;
			complete_transaction();
			break;
		case State::FILTERING_TEST_2:
			perform_filtering_test3();
			break;
		case State::FILTERING_TEST_3:
			tx_->data.behaviour_result = Behaviour::ADDRESS_PORT_DEPENDENT;
			complete_transaction();
			break;
		default:
			abort_transaction(Error::NO_RESPONSE_RECEIVED);
	}
}

void Client::start_transaction_timer() {
	tx_->timer.expires_from_now(tx_->timeout);
	tx_->timer.async_wait([&](const boost::system::error_code& ec) {
		if(ec) {
			return;
		}

		std::lock_guard guard(mutex_);

		// RFC algorithm decided by interpretive dance
		if(tx_->retries_left) {
			if (tx_->retries_left != tx_->max_retries) {
				tx_->timeout *= 2;
			}
				
			--tx_->retries_left;

			if(!tx_->retries_left) {
				tx_->timeout = tx_->initial_to * TX_RM;
			}

			start_transaction_timer();
			transport_->send(tx_->retry_buffer);
		} else {
			handle_no_response();
		}
	});
}

void Client::on_connection_error(const boost::system::error_code& ec) {
	std::lock_guard guard(mutex_);

	if(ec == boost::asio::error::connection_aborted) {
		abort_transaction(Error::CONNECTION_ABORTED);
	} else if(ec == boost::asio::error::connection_reset) {
		abort_transaction(Error::CONNECTION_RESET);
	} else {
		abort_transaction(Error::CONNECTION_ERROR);
	}
}

void Client::options(const clientopts opts) {
	opts_ = opts; 
}

clientopts Client::options() const {
	return opts_;
}

void Client::perform_connectivity_test() {
	auto [buffer, key] = build_request();  
	tx_->retry_buffer = buffer;
	tx_->key = key;
	transport_->send(buffer);
	start_transaction_timer();
}

std::future<NATResult> Client::nat_present() {
	std::promise<NATResult> promise;
	auto future = promise.get_future();

	if(is_nat_present_) {
		promise.set_value(*is_nat_present_);
		return future;
	}

	std::lock_guard guard(mutex_);
	create_transaction(std::move(promise));

	connect(host_, port_, [&](const boost::system::error_code& ec) {
		if(!ec) {
			tx_->state = State::BASIC;
			perform_connectivity_test();
		} else {
			abort_transaction(Error::UNABLE_TO_CONNECT);
		}
	});

	return future;
}

template<typename T>
std::future<T> Client::basic_request() {
	std::promise<T> promise;
	auto future = promise.get_future();

	std::lock_guard guard(mutex_);
	create_transaction(std::move(promise));

	connect(host_, port_, [&](const boost::system::error_code&) {
		tx_->state = State::BASIC;
		perform_connectivity_test();
	});

	return future;
}

template<typename T>
std::future<T> Client::behaviour_test(const State state) {
	std::promise<T> promise;
	auto future = promise.get_future();

	if(proto_ == Protocol::TCP) {
		promise.set_value(std::unexpected(ErrorRet(Error::UDP_TEST_ONLY)));
		return future;
	}

	std::lock_guard guard(mutex_);
	create_transaction(std::move(promise));
	tx_->state = state;

	connect(host_, port_, [&](const boost::system::error_code&) {
		perform_connectivity_test();
	});

	return future;
}


std::future<BehaviourResult> Client::filtering() {
	return behaviour_test<BehaviourResult>(State::FILTERING_TEST_1);
}

std::future<BehaviourResult> Client::mapping() {
	return behaviour_test<BehaviourResult>(State::MAPPING_TEST_1);
}

// RFC allows it over TCP but we don't support it
std::future<HairpinResult> Client::hairpinning() {
	return behaviour_test<HairpinResult>(State::HAIRPIN);
}

std::future<AttributesResult> Client::binding_request() {
	return basic_request<AttributesResult>();
}

std::future<MappedResult> Client::external_address() {
	return basic_request<MappedResult>();
}

} // stun, ember