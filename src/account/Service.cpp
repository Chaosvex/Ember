/*
 * Copyright (c) 2015 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Service.h"
#include <spark/temp/MessageRoot_generated.h>

namespace em = ember::messaging;

namespace ember {

Service::Service(Sessions& sessions, spark::Service& spark, spark::ServiceDiscovery& discovery, log::Logger* logger)
                 : sessions_(sessions), spark_(spark), discovery_(discovery), logger_(logger) {
	spark_.dispatcher()->register_handler(this, em::Service::Account, spark::EventDispatcher::Mode::SERVER);
	discovery_.register_service(em::Service::Account);
}

Service::~Service() {
	discovery_.remove_service(em::Service::Account);
	spark_.dispatcher()->remove_handler(this);
}

void Service::handle_message(const spark::Link& link, const em::MessageRoot* msg) {
	switch(msg->data_type()) {
		case em::Data::RegisterKey:
			register_session(link, msg);
			break;
		case em::Data::KeyLookup:
			locate_session(link, msg);
			break;
		default:
			LOG_DEBUG(logger_) << "Service received unhandled message type" << LOG_ASYNC;
	}
}

void Service::register_session(const spark::Link& link, const em::MessageRoot* root) {
	auto msg = static_cast<const em::account::RegisterKey*>(root->data());
	auto status = em::account::Status::OK;
	
	Botan::BigInt key(msg->key()->data(), msg->key()->size());
	bool res = sessions_.register_session(msg->account_id(), key);

	if(!res) {
		status = em::account::Status::ALREADY_LOGGED_IN;
	}

	send_register_reply(link, root, status);
}

void Service::locate_session(const spark::Link& link, const em::MessageRoot* root) {
	auto msg = static_cast<const em::account::KeyLookup*>(root->data());
	auto session = sessions_.lookup_session(msg->account_id());
	send_locate_reply(link, root, session);
}

void Service::send_register_reply(const spark::Link& link, const em::MessageRoot* root,
                                  em::account::Status status) {
	auto fbb = std::make_shared<flatbuffers::FlatBufferBuilder>();
	em::account::ResponseBuilder rb(*fbb);
	rb.add_status(status);
	auto data_offset = rb.Finish();

	em::MessageRootBuilder mrb(*fbb);
	mrb.add_service(em::Service::Account);
	mrb.add_data_type(em::Data::Response);
	mrb.add_data(data_offset.Union());
	set_tracking_data(root, mrb, fbb.get());
	auto mloc = mrb.Finish();

	fbb->Finish(mloc);
	spark_.send(link, fbb);
}

void Service::send_locate_reply(const spark::Link& link, const em::MessageRoot* root,
                                const boost::optional<Botan::BigInt>& key) {
	auto msg = static_cast<const em::account::KeyLookup*>(root->data());

	auto fbb = std::make_shared<flatbuffers::FlatBufferBuilder>();
	em::account::KeyLookupRespBuilder klb(*fbb);

	if(key) {
		auto encoded_key = Botan::BigInt::encode(*key);
		klb.add_key(fbb->CreateVector(encoded_key.begin(), encoded_key.size()));
		klb.add_status(em::account::Status::OK);
	} else {
		klb.add_status(em::account::Status::SESSION_NOT_FOUND);
	}

	klb.add_account_id(msg->account_id());
	auto data_offset = klb.Finish();

	em::MessageRootBuilder mrb(*fbb);
	mrb.add_service(em::Service::Account);
	mrb.add_data_type(em::Data::KeyLookupResp);
	mrb.add_data(data_offset.Union());
	set_tracking_data(root, mrb, fbb.get());
	auto mloc = mrb.Finish();

	fbb->Finish(mloc);
	spark_.send(link, fbb);
}

void Service::handle_link_event(const spark::Link& link, spark::LinkState event) {
	switch(event) {
		case spark::LinkState::LINK_UP:
			LOG_DEBUG(logger_) << "Link up: " << link.description << LOG_ASYNC;
			break;
		case spark::LinkState::LINK_DOWN:
			LOG_DEBUG(logger_) << "Link down: " << link.description << LOG_ASYNC;
			break;
	}
}

void Service::set_tracking_data(const em::MessageRoot* root, em::MessageRootBuilder& mrb,
								flatbuffers::FlatBufferBuilder* fbb) {
	if(root->tracking_id()) {
		auto id = fbb->CreateVector(root->tracking_id()->data(), root->tracking_id()->size());
		mrb.add_tracking_id(id);
		mrb.add_tracking_ttl(1);
	}
}

} // ember