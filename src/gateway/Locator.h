/*
 * Copyright (c) 2016 - 2024 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once 

namespace ember::gateway {

class EventDispatcher;
class CharacterClient;
class AccountClient;
class RealmService;
class RealmQueue;
struct Config;

class Locator {
	static EventDispatcher* dispatcher_;
	static CharacterClient* character_;
	static AccountClient* account_;
	static RealmService* realm_;
	static RealmQueue* queue_;
	static Config* config_;

public:
	static void set(Config* config) { config_ = config; }
	static void set(RealmQueue* queue) { queue_ = queue; }
	static void set(RealmService* realm) { realm_ = realm; }
	static void set(AccountClient* account) { account_ = account; }
	static void set(CharacterClient* character) { character_ = character; }
	static void set(EventDispatcher* dispatcher) { dispatcher_ = dispatcher; }

	static Config* config() { return config_; }
	static RealmQueue* queue() { return queue_; }
	static RealmService* realm() { return realm_; }
	static AccountClient* account() { return account_; }
	static CharacterClient* character() { return character_; }
	static EventDispatcher* dispatcher() { return dispatcher_; }
};

} // gateway, ember