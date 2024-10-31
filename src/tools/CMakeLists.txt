# Copyright (c) 2014 - 2024 Ember
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

add_subdirectory(dbcparser)
add_subdirectory(dbutils)
add_subdirectory(rpcgen)

if(BUILD_OPT_TOOLS)
    add_subdirectory(packetconvert)
    add_subdirectory(srpgen)
    add_subdirectory(stun)
    add_subdirectory(portopen)
	add_subdirectory(mpqextract)
endif()