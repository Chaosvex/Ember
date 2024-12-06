# Copyright (c) 2015 - 2024 Ember
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(LIBRARY_NAME conpool)

add_library(${LIBRARY_NAME}
            src/MySQL/Driver.cpp
            src/MySQL/Config.cpp
			src/DummyDriver.cpp
            include/conpool/ConnectionPool.h
            include/conpool/PoolManager.h
            include/conpool/Connection.h
            include/conpool/Policies.h
            include/conpool/Exception.h
            include/conpool/LogSeverity.h
            include/conpool/drivers/AutoSelect.h
            include/conpool/drivers/MySQL/Driver.h
            include/conpool/drivers/MySQL/Config.h
			include/conpool/drivers/DummyDriver.h
           )

target_link_libraries(${LIBRARY_NAME} shared ${MYSQLCCPP_LIBRARY})
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(conpool PROPERTIES FOLDER "Libraries")