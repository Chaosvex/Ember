# Copyright (c) 2015 - 2024 Ember
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(LIBRARY_NAME shared)

set(DB_OBJECTS
    shared/database/objects/User.h
    shared/database/objects/Character.h
    shared/database/objects/PatchMeta.h
)

set(SHARED_DAOS_SRC
    shared/database/Exception.h
    shared/database/daos/IPBanDAO.h
    shared/database/daos/RealmDAO.h
    shared/database/daos/UserDAO.h
    shared/database/daos/CharacterDAO.h
    shared/database/daos/PatchDAO.h
    shared/database/daos/shared_base/IPBanBase.h
    shared/database/daos/shared_base/RealmBase.h
    shared/database/daos/shared_base/UserBase.h
    shared/database/daos/shared_base/CharacterBase.h
    shared/database/daos/shared_base/PatchBase.h
)

set(MYSQL_DAOS_SRC
    shared/database/daos/mysql/IPBanDAO.h
    shared/database/daos/mysql/RealmDAO.h
    shared/database/daos/mysql/UserDAO.h
    shared/database/daos/mysql/CharacterDAO.h
    shared/database/daos/mysql/PatchDAO.h
)

set(POSTGRESQL_DAOS_SRC
    shared/database/daos/postgresql/IPBanDAO.h
    shared/database/daos/postgresql/RealmDAO.h
    shared/database/daos/postgresql/UserDAO.h
    shared/database/daos/postgresql/CharacterDAO.h
    shared/database/daos/postgresql/PatchDAO.h
)

set(MEMORY_SRC
    shared/memory/ASIOAllocator.h
)

set(THREADING_SRC
    shared/threading/Semaphore.h
    shared/threading/Spinlock.h
    shared/threading/ThreadPool.cpp
    shared/threading/ThreadPool.h
    shared/threading/Utility.h
    shared/threading/Utility.cpp
    shared/threading/ServicePool.h
    shared/threading/ServicePool.cpp
)

set(UTIL_SRC
    shared/util/xoroshiro128plus.h
    shared/util/xoroshiro128plus.cpp
    shared/util/enum_bitmask.h
    shared/util/PCREHelper.h
    shared/util/PCREHelper.cpp
    shared/util/UTF8String.h
    shared/util/UTF8.h
    shared/util/UTF8.cpp
    shared/util/Utility.h
    shared/util/Utility.cpp
    shared/util/FormatPacket.h
    shared/util/LogConfig.cpp
    shared/util/LogConfig.h
    shared/util/ConsoleColour.h
    shared/util/ConsoleColour.cpp
    shared/util/base32.h
    shared/util/base32.cpp
    shared/util/FileMD5.h
    shared/util/FileMD5.cpp
    shared/util/FNVHash.h
    shared/util/EnumHelper.h
    shared/util/MulticharConstant.h
    shared/util/Clock.h
    shared/util/StringHash.h
    shared/util/STUN.h
	shared/util/PortForward.h
    shared/util/polyfill/print
    shared/util/polyfill/start_lifetime_as
    shared/util/polyfill/inplace_vector
    shared/util/Timing.h
    shared/util/Timing.cpp
)

set(METRICS_SRC
    shared/metrics/Metrics.h
    shared/metrics/MetricsImpl.h
    shared/metrics/MetricsImpl.cpp
    shared/metrics/Monitor.h
    shared/metrics/Monitor.cpp
    shared/metrics/MetricsPoll.h
    shared/metrics/MetricsPoll.cpp
)

set(LIBRARY_SRC
    ${DB_OBJECTS}
    ${SHARED_DAOS_SRC}
    ${MYSQL_DAOS_SRC}
    ${POSTGRESQL_DAOS_SRC}
    ${MEMORY_SRC}
    ${THREADING_SRC}
    ${UTIL_SRC}
    ${METRICS_SRC}
    shared/ClientRef.h
    shared/CompilerWarn.h
    shared/IPBanCache.h
    shared/Realm.h
    shared/Banner.cpp
    shared/Banner.h
    shared/Version.h
    shared/smartenum.hpp
    shared/smartenum.cpp
    ${version_file}
)

source_group("Database Objects" FILES ${DB_OBJECTS})
source_group("DAO base" FILES ${SHARED_DAOS_SRC})
source_group("MySQL DAO" FILES ${MYSQL_DAOS_SRC})
source_group("PostgreSQL DAO" FILES ${POSTGRESQL_DAOS_SRC})
source_group("Memory" FILES ${MEMORY_SRC})
source_group("Threading" FILES ${THREADING_SRC})
source_group("Utilities" FILES ${UTIL_SRC})
source_group("Metrics" FILES ${METRICS_SRC})

include_directories(${CMAKE_SOURCE_DIR}/deps/utf8cpp ${PROJECT_BINARY_DIR}/src)
add_library(${LIBRARY_NAME} ${LIBRARY_SRC})
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/)
target_link_libraries(${LIBRARY_NAME} logger conpool ${PCRE_LIBRARY})
set_target_properties(shared PROPERTIES FOLDER "Libraries")