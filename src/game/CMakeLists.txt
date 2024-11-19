# Copyright (c) 2016 - 2024 Ember
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(LIBRARY_NAME    game)

set(LIBRARY_HDR
    Weather.h
    )

set(LIBRARY_SRC
    Weather.cpp
    )

add_library(${LIBRARY_NAME} ${LIBRARY_HDR} ${LIBRARY_SRC})
target_link_libraries(${LIBRARY_NAME} spark logger shared ${Boost_LIBRARIES})
set_target_properties(game PROPERTIES FOLDER "Services")