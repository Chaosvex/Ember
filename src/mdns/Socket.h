/*
 * Copyright (c) 2021 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

namespace ember::dns {

class Handler;

class Socket {
public:
    virtual void send() = 0;
    virtual void register_handler(Handler*) = 0;
    virtual ~Socket() = default;
};

} // dns, ember