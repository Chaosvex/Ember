# Copyright (c) 2024 Ember
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(EXECUTABLE_NAME fusion)

add_executable(${EXECUTABLE_NAME} main.cpp)

set(SERVICE_LIBRARIES libworld liblogin libgateway libaccount libmdns libcharacter)
target_link_libraries(${EXECUTABLE_NAME} ${LIBRARY_NAME} logger shared ${SERVICE_LIBRARIES} ${Boost_LIBRARIES})
target_include_directories(${EXECUTABLE_NAME} PRIVATE ../)

INSTALL(TARGETS ${EXECUTABLE_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
set_target_properties(fusion PROPERTIES FOLDER "Services")