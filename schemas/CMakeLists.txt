# Copyright (c) 2016 - 2024 Ember
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(FB_SCHEMA_TARGET_NAME FB_SCHEMA_COMPILE)

set(SERVICE_SCHEMAS
    ${CMAKE_CURRENT_SOURCE_DIR}/spark/services/Account.fbs
    ${CMAKE_CURRENT_SOURCE_DIR}/spark/services/Character.fbs
    ${CMAKE_CURRENT_SOURCE_DIR}/spark/services/Realm.fbs
    ${CMAKE_CURRENT_SOURCE_DIR}/spark/services/Discovery.fbs
)

set(FB_SCHEMAS
	${SERVICE_SCHEMAS}
    ${CMAKE_CURRENT_SOURCE_DIR}/spark/Spark.fbs
    ${CMAKE_CURRENT_SOURCE_DIR}/packetlog/PacketLog.fbs
)

set(FLATC_ARGS "-b" "-c" "--gen-mutable" "--scoped-enums" "--schema" "--bfbs-comments" "--gen-object-api")
build_flatbuffers("${FB_SCHEMAS}" ${CMAKE_CURRENT_SOURCE_DIR} ${FB_SCHEMA_TARGET_NAME} "" ${CMAKE_BINARY_DIR} "" "")

set(TEMPLATE_DIR ${CMAKE_SOURCE_DIR}/src/tools/rpcgen/templates/)
build_spark_services("${SERVICE_SCHEMAS}" ${TEMPLATE_DIR} ${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR})