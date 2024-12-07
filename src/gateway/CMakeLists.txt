# Copyright (c) 2016 - 2024 Ember
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(EXECUTABLE_NAME gateway)
set(LIBRARY_NAME    libgateway)

set(LIBRARY_HDR
    Config.h
    Event.h
    EventDispatcher.h
    Events.h
    EventTypes.h
    Locator.h
    SessionManager.h
    FilterTypes.h
    RealmService.h
    NetworkListener.h
    ClientConnection.h
    ClientConnection.inl
    AccountClient.h
    RealmQueue.h
    ClientHandler.h
    ClientHandler.inl
    PacketCrypto.h
    ConnectionStats.h
    ServerConfig.h
    QoS.h
    WorldConnection.h
    WorldSessions.h
    WorldClients.h
    CharacterClient.h
    CompressMessage.h
    Routing.h
    ClientLogHelper.h
	ConnectionDefines.h
    SocketType.h
    states/ClientStates.h
    states/Authentication.h
    states/CharacterList.h
    states/WorldForwarder.h
    states/SessionClose.h
    states/WorldTransfer.h
    states/ClientContext.h
    states/StateJumpTables.h
    states/AuthenticationContext.h
    states/WorldEnter.h
    states/WorldEnterContext.h
    packetlog/PacketLogger.h
    packetlog/PacketSink.h
    packetlog/FBSink.h
    packetlog/LogSink.h
    Runner.h
    )

set(LIBRARY_SRC
    EventDispatcher.cpp
    Locator.cpp
    SessionManager.cpp
    ClientConnection.cpp
    RealmService.cpp
    AccountClient.cpp
    RealmQueue.cpp
    ClientHandler.cpp
    QoS.cpp
    WorldConnection.cpp
    WorldSessions.cpp
    WorldClients.cpp
    CharacterClient.cpp
    CompressMessage.cpp
    NetworkListener.cpp
    states/Authentication.cpp
    states/CharacterList.cpp
    states/WorldForwarder.cpp
    states/SessionClose.cpp
    states/WorldEnter.cpp
    packetlog/PacketLogger.cpp
    packetlog/FBSink.cpp
    packetlog/LogSink.cpp
    Runner.cpp
    )

add_library(${LIBRARY_NAME} ${LIBRARY_HDR} ${LIBRARY_SRC})
add_dependencies(${LIBRARY_NAME} FB_SCHEMA_COMPILE)
target_link_libraries(${LIBRARY_NAME} nsd conpool stun ports dbcreader protocol spark logger shared  ${ZLIB_LIBRARY} ${BOTAN_LIBRARY} ${Boost_LIBRARIES} Threads::Threads)

add_executable(${EXECUTABLE_NAME} main.cpp)
target_link_libraries(${EXECUTABLE_NAME} ${LIBRARY_NAME} logger shared ${Boost_LIBRARIES} Threads::Threads)

INSTALL(TARGETS ${EXECUTABLE_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
set_target_properties(gateway libgateway PROPERTIES FOLDER "Services")