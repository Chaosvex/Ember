/*
* Copyright (c) 2019 Ember
*
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/*
 * At the moment, this only exists to make it clear which strings hold
 * UTF8 content to decrease the odds of making mistakes with size/length, etc.
 */
#pragma once

#include <string>

namespace ember {

using utf8_string = std::string;

} // ember