# Copyright (c) 2015 - 2024 Ember
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(LIBRARY_NAME spark)

set(CORE
    include/spark/Server.h
    include/spark/Handler.h
    include/spark/Connection.h
    include/spark/RemotePeer.h
    include/spark/Link.h
    include/spark/MessageHeader.h
    include/spark/Channel.h
    include/spark/HandlerRegistry.h
    include/spark/Peers.h
    include/spark/Utility.h
    include/spark/Tracking.h
    include/spark/Common.h
    include/spark/Result.h
    src/Peers.cpp
    src/Server.cpp
    src/RemotePeer.cpp
    src/Connection.cpp
    src/Channel.cpp
    src/HandlerRegistry.cpp
    src/Tracking.cpp
)

set(IO_SRC
    include/spark/buffers/Exception.h
    include/spark/buffers/DynamicBuffer.h
    include/spark/buffers/DynamicTLSBuffer.h
    include/spark/buffers/IntrusiveStorage.h
    include/spark/buffers/SharedDefs.h
    include/spark/buffers/BufferAdaptor.h
    include/spark/buffers/BufferSequence.h
    include/spark/buffers/BinaryStream.h
    include/spark/buffers/StaticBuffer.h
)

set(IO_PMR_SRC
    include/spark/buffers/pmr/BufferBase.h
    include/spark/buffers/pmr/BufferRead.h
    include/spark/buffers/pmr/BufferWrite.h
    include/spark/buffers/pmr/Buffer.h
    include/spark/buffers/pmr/NullBuffer.h
    include/spark/buffers/pmr/BinaryStream.h
    include/spark/buffers/pmr/BinaryStreamReader.h
    include/spark/buffers/pmr/BinaryStreamWriter.h
    include/spark/buffers/pmr/StreamBase.h
    include/spark/buffers/pmr/BufferAdaptor.h
    include/spark/buffers/pmr/BufferReadAdaptor.h
    include/spark/buffers/pmr/BufferWriteAdaptor.h
)

set(IO_ALLOC_SRC
    include/spark/buffers/allocators/DefaultAllocator.h
    include/spark/buffers/allocators/TLSBlockAllocator.h
)

add_library(
    ${LIBRARY_NAME}
    ${CORE}
    ${IO_SRC}
    ${IO_PMR_SRC}
	${IO_ALLOC_SRC}
)

source_group("Core" FILES ${CORE})
source_group("IO" FILES ${IO_SRC})
source_group("IO\\Polymorphic" FILES ${IO_PMR_SRC})
source_group("IO\\Allocators" FILES ${IO_ALLOC_SRC})

add_dependencies(${LIBRARY_NAME} FB_SCHEMA_COMPILE)
target_link_libraries(${LIBRARY_NAME} shared ${Boost_LIBRARIES})
target_include_directories(${LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(spark PROPERTIES FOLDER "Libraries")