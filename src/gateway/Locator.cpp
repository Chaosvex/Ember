/*
 * Copyright (c) 2016 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "Locator.h"

namespace ember {

CharacterService* Locator::character_;
AccountService* Locator::account_;
RealmService* Locator::realm_;
RealmQueue* Locator::queue_;

} // ember
