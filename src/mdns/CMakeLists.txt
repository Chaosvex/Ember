# Copyright (c) 2021 - 2024 Ember
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(EXECUTABLE_NAME mdns)
set(LIBRARY_NAME    libmdns)

set(LIBRARY_HDR
    ServiceRegistry.h
    MulticastSocket.h
    Server.h
    Socket.h
    DNSDefines.h
    Handler.h
	Utility.h
	Serialisation.h
	detail/Parser.h
	NSDService.h
	Runner.h
    )

set(LIBRARY_SRC
    ServiceRegistry.cpp
    MulticastSocket.cpp
    Server.cpp
	Utility.cpp
	Serialisation.cpp
	detail/Parser.cpp
	NSDService.cpp
	Runner.cpp
    )

add_library(${LIBRARY_NAME} ${LIBRARY_HDR} ${LIBRARY_SRC})
add_dependencies(${LIBRARY_NAME} FB_SCHEMA_COMPILE)
target_link_libraries(${LIBRARY_NAME} logger shared spark ${Boost_LIBRARIES} Threads::Threads)

add_executable(${EXECUTABLE_NAME} main.cpp)
target_link_libraries(${EXECUTABLE_NAME} ${LIBRARY_NAME} logger shared ${Boost_LIBRARIES} Threads::Threads)

INSTALL(TARGETS ${EXECUTABLE_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
set_target_properties(mdns libmdns PROPERTIES FOLDER "Services")