/*
 * Copyright (c) 2015 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <logger/Worker.h>
#include <shared/threading/Utility.h>
#include <iterator>

namespace ember::log {

Worker::~Worker() {
	if(!stop_) {
		stop();
	}
}

void Worker::process_outstanding_sync() {
	std::tuple<RecordDetail, std::vector<char>, std::binary_semaphore*> item;

	while(queue_sync_.try_dequeue(item)) {
		for(auto& s : sinks_) {
			s->write(std::get<0>(item).severity, std::get<0>(item).type, std::get<1>(item), true);
		}
		std::get<2>(item)->release();
	}
}

void Worker::process_outstanding() {
	if(!queue_.try_dequeue_bulk(std::back_inserter(dequeued_), queue_.size_approx())) {
		return;
	}
		
	std::size_t records = dequeued_.size();

	if(records < 5) {
		for(auto& s : sinks_) {
			for(auto& [detail, data] : dequeued_) {
				s->write(detail.severity, detail.type, data, false);
			}
		}
	} else {
		for(auto& s : sinks_) {
			s->batch_write(dequeued_);
		}
	}

	dequeued_.clear();

	if(dequeued_.capacity() > 100 && records < 100) {
		dequeued_.shrink_to_fit();
	}
}

void Worker::run() {
#ifndef DEBUG_NO_THREADS
	while(!stop_) {
#endif
		sem_.acquire();
		process_outstanding();
		process_outstanding_sync();
#ifndef DEBUG_NO_THREADS
	}
#endif
}

void Worker::start() {
	thread_ = std::thread(&Worker::run, this);
	thread::set_name(thread_, "Log Worker");
}

void Worker::stop() {
	if(thread_.joinable()) {
		stop_ = true;
		sem_.release();
		thread_.join();
		process_outstanding();
		process_outstanding_sync();
	}
}

} //log, ember