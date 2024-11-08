/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "RealmQueue.h"

namespace ember {

void RealmQueue::set_timer() {
	timer_.expires_from_now(frequency_);
	timer_.async_wait([this](const boost::system::error_code& ec) {
		if(!ec) { // if ec is set, the timer was aborted (shutdown)
			update_clients();
		}
	});
}

/*
 * Periodically update clients with their current queue position
 * This is done with a timer rather than as players leave the queue/server
 * in order to reduce network traffic with longer queues where queue positions
 * are changing rapidly
 */
void RealmQueue::update_clients() {
	std::lock_guard guard(lock_);

	if(!dirty_) {
		return;
	}

	std::size_t position = 1;

	for(auto& entry : queue_) {
		entry.on_update(position);
		++position;
	}

	set_timer();
	dirty_ = false;
}

void RealmQueue::enqueue(ClientRef client, UpdateQueueCB on_update_cb,
                         LeaveQueueCB on_leave_cb, int priority) {
	std::lock_guard guard(lock_);

	if(queue_.empty()) {
		set_timer();
	}

	queue_.emplace_back(priority, client, on_update_cb, on_leave_cb);

	// guaranteed to be a stable sort - not the most efficient way to have queue priority
	// but allows for multiple priority levels without multiple hard-coded queues
	queue_.sort();
	dirty_ = true;
}

/* 
 * Signals that a currently queued player has decided to disconnect rather
 * hang around in the queue
 */
void RealmQueue::dequeue(const ClientRef& client) {
	std::lock_guard guard(lock_);

	if(auto it = std::ranges::find(queue_, client, &QueueEntry::client); it != queue_.end()) {
		queue_.erase(it);
	}

	if(queue_.empty()) {
		timer_.cancel();
	}

	dirty_ = true;
}

/* 
 * Signals that a player occupying a server slot has disconnected, thus
 * allowing the player at the front of the queue to connect
 */
void RealmQueue::free_slot() {
	std::lock_guard guard(lock_);

	if(queue_.empty()) {
		return;
	}

	auto& entry = queue_.front();
	entry.on_leave();
	queue_.pop_front();

	if(queue_.empty()) {
		timer_.cancel();
	}

	dirty_ = true;
}

void RealmQueue::shutdown() {
	std::lock_guard guard(lock_);
	timer_.cancel();
}

std::size_t RealmQueue::size() const {
	return queue_.size();
}

} // ember