# Copyright (c) 2016 - 2024 Ember
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(LIBRARY_NAME dbcreader)

set(DBC_DEFINITION_DIRS
    ${CMAKE_SOURCE_DIR}/dbcs/definitions/server
    ${CMAKE_SOURCE_DIR}/dbcs/definitions/client
)

set(DBC_TARGET_NAME      DBC_DEFINITION_COMPILE)
set(CODE_TEMPLATE_DIR    ${CMAKE_SOURCE_DIR}/src/tools/dbcparser/templates/)
set(CODE_OUTPUT_DIR      ${PROJECT_BINARY_DIR}/src/dbcreader)
set(FILE_LOG_VERBOSITY   info)

build_dbc_loaders(DBC_HDR DBC_SRC
                 "${DBC_DEFINITION_DIRS}"
                  ${CODE_OUTPUT_DIR}
                  ${CODE_TEMPLATE_DIR}
                  ${DBC_TARGET_NAME}
                  ${FILE_LOG_VERBOSITY})

add_library(${LIBRARY_NAME} ${DBC_HDR} ${DBC_SRC}
            include/dbcreader/Reader.h
            include/dbcreader/Header.h
            include/dbcreader/Store.h
            include/dbcreader/DiskLoader.h
            include/dbcreader/Linker.h
            include/dbcreader/Loader.h
           )

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${LIBRARY_NAME} shared ${Boost_LIBRARIES})
target_include_directories(${LIBRARY_NAME} PUBLIC shared ${PROJECT_BINARY_DIR}/src ${PROJECT_BINARY_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(dbcreader PROPERTIES FOLDER "Libraries")