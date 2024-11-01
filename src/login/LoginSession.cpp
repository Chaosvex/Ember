/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "LoginSession.h"
#include "LoginHandlerBuilder.h"
#include "FilterTypes.h"
#include <logger/Logger.h>
#include <shared/metrics/Metrics.h>
#include <shared/threading/ThreadPool.h>
#include <boost/asio/post.hpp>
#include <exception>
#include <functional>
#include <memory>
#include <utility>

namespace ember {

LoginSession::LoginSession(SessionManager& sessions, tcp_socket socket, log::Logger& logger,
                           ThreadPool& pool, const LoginHandlerBuilder& builder)
                           : NetworkSession(sessions, std::move(socket), logger),
                             handler_(builder.create(remote_address())),
                             logger_(logger),
                             pool_(pool),
                             grunt_handler_(logger) {
	handler_.send = [&](auto& packet) {
		write_packet(packet, nullptr);
	};

	handler_.send_cb = [&](auto& packet, auto cb) mutable {
		write_packet(packet, std::move(cb));
	};

	handler_.execute_async = [&](auto action) {
		execute_async(std::move(action));
	};
}

bool LoginSession::handle_packet(spark::io::pmr::Buffer& buffer) try {
	LOG_TRACE_FILTER(logger_, LF_NETWORK) << log_func << LOG_ASYNC;

	auto result = grunt_handler_.process_buffer(buffer);

	if(result) {
		const auto& packet = result->get();
		LOG_TRACE_FILTER(logger_, LF_NETWORK) << remote_address() << " -> "
			<< grunt::to_string(packet.opcode) << LOG_ASYNC;
		return handler_.update_state(packet);
	}

	return true;
} catch(grunt::bad_packet& e) {
	LOG_DEBUG_FILTER(logger_, LF_NETWORK) << e.what() << LOG_ASYNC;
	return false;
}

void LoginSession::execute_async(std::unique_ptr<Action> action) {
	LOG_TRACE_FILTER(logger_, LF_NETWORK) << log_func << LOG_ASYNC;

	auto self(shared_from_this());
	std::shared_ptr<Action> shared_act(std::move(action));

	pool_.run([&, action = std::move(shared_act), self]() mutable {
		action->execute();

		boost::asio::post(get_executor(), [&, action = std::move(action), self] {
			if(!is_stopped()) {
				async_completion(*action.get());
			}
		});
	});
}

void LoginSession::async_completion(Action& action) try {
	LOG_TRACE_FILTER(logger_, LF_NETWORK) << log_func << LOG_ASYNC;

	if(!handler_.update_state(action)) {
		close_session(); // todo change
	}
} catch(const std::exception& e) {
	LOG_DEBUG(logger_) << e.what() << LOG_ASYNC;
	close_session();
}

void LoginSession::write_packet(const grunt::Packet& packet, WriteCallback&& cb) {
	LOG_TRACE_FILTER(logger_, LF_NETWORK) << log_func << LOG_ASYNC;

	LOG_TRACE_FILTER(logger_, LF_NETWORK) << remote_address() << " <- "
		<< grunt::to_string(packet.opcode) << LOG_ASYNC;

	write(packet, std::move(cb));
}

} // ember