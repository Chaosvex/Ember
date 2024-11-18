# Copyright (c) 2016 - 2024 Ember
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(EXECUTABLE_NAME world)
set(LIBRARY_NAME    libworld)

set(LIBRARY_HDR
    Launch.h
    MapRunner.h
    Watchdog.h
    utilities/DeltaTimer.h
    utilities/Utility.h
    )

set(LIBRARY_SRC
    Launch.cpp
    MapRunner.cpp
    Watchdog.cpp
    utilities/DeltaTimer.cpp
    utilities/Utility.cpp
    )

add_library(${LIBRARY_NAME} ${LIBRARY_HDR} ${LIBRARY_SRC})
target_link_libraries(${LIBRARY_NAME} dbcreader spark logger shared ${Boost_LIBRARIES})

add_executable(${EXECUTABLE_NAME} main.cpp)
target_link_libraries(${EXECUTABLE_NAME} ${LIBRARY_NAME} logger shared ${Boost_LIBRARIES})

INSTALL(TARGETS ${EXECUTABLE_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
set_target_properties(world libworld PROPERTIES FOLDER "Services")