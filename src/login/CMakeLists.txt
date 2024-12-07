# Copyright (c) 2014 - 2024 Ember
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(EXECUTABLE_NAME login)
set(LIBRARY_NAME    liblogin)

set(GRUNT_SRC
    grunt/Magic.h
    grunt/Exceptions.h
    grunt/ResultCodes.h
    grunt/Handler.h
    grunt/Handler.cpp
    grunt/Packets.h
    grunt/Packet.h
    grunt/PacketFwd.h
    grunt/Opcodes.h
    grunt/client/LoginChallenge.h
    grunt/client/LoginProof.h
    grunt/client/ReconnectProof.h
    grunt/client/RequestRealmList.h
    grunt/client/SurveyResult.h
    grunt/client/TransferAccept.h
    grunt/client/TransferResume.h
    grunt/client/TransferCancel.h
    grunt/server/LoginChallenge.h
    grunt/server/ReconnectChallenge.h
    grunt/server/LoginProof.h
    grunt/server/ReconnectProof.h
    grunt/server/RealmList.h
    grunt/server/TransferData.h
    grunt/server/TransferInitiate.h
)

set(LIBRARY_HDR
    Actions.h
    SessionBuilders.h
    LoginSession.h
    NetworkListener.h
    NetworkSession.h
    LoginHandler.h
    LoginHandlerBuilder.h
    SessionManager.h
    GameVersion.h
    Authenticator.h
    RealmList.h
    Actions.h
    Patcher.h
    FilterTypes.h
    MonitorCallbacks.h
    AccountClient.h
    RealmClient.h
    PINAuthenticator.h
    ExecutablesChecksum.h
    PatchGraph.h
    IntegrityData.h
    IntegrityPlatforms.h
    LocaleMap.h
	LoginState.h
	Survey.h
    LoginHandlerFwd.h
    SocketType.h
    Runner.h
    )

set(LIBRARY_SRC
    LoginHandler.cpp
    SessionManager.cpp
    LoginSession.cpp
    RealmList.cpp
    Authenticator.cpp
    Patcher.cpp
    MonitorCallbacks.cpp
    AccountClient.cpp
    RealmClient.cpp
    PINAuthenticator.cpp
    ExecutablesChecksum.cpp
    PatchGraph.cpp
    IntegrityData.cpp
    LocaleMap.cpp
	Survey.cpp
    Runner.cpp
    )

source_group("Grunt Protocol" FILES ${GRUNT_SRC})

add_library(${LIBRARY_NAME} ${LIBRARY_HDR} ${LIBRARY_SRC} ${GRUNT_SRC})
add_dependencies(${LIBRARY_NAME} FB_SCHEMA_COMPILE)
target_link_libraries(${LIBRARY_NAME} dbcreader spark srp6 logger stun ports shared ${ZLIB_LIBRARY} ${BOTAN_LIBRARY} ${Boost_LIBRARIES} Threads::Threads)

add_executable(${EXECUTABLE_NAME} main.cpp)
target_link_libraries(${EXECUTABLE_NAME} ${LIBRARY_NAME} logger shared ${Boost_LIBRARIES} Threads::Threads)

INSTALL(TARGETS ${EXECUTABLE_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
set_target_properties(login liblogin PROPERTIES FOLDER "Services")