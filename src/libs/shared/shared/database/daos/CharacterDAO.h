/*
 * Copyright (c) 2016 Ember
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#ifdef DB_MYSQL
	#include <shared/database/daos/mysql/CharacterDAO.h>
#elif DB_POSTGRESQL
 	#include <shared/database/daos/postgresql/CharacterDAO.h>
#endif