cmake_minimum_required(VERSION 3.10)
project(zmq_task_dispatcher CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

SET(ENABLE_TRACY OFF CACHE BOOL "Enable/Disable Tracy Profiler")

find_package(ZeroMQ REQUIRED)

if(ENABLE_TRACY)
    find_library(TRACY_LIBRARY NAMES tracy)
endif()

SET(HeaderFiles 
    src/jsonrpc_handler.hpp 
    src/message_dispatcher.hpp
    src/utils.hpp
    src/zmq_server.hpp
    )

SET(SourceFiles 
    src/main.cpp
    src/zmq_server.cpp
    src/message_dispatcher.cpp
    src/jsonrpc_handler.cpp
    )

SET(3rdPartyFiles 
    src/deps/cppzmq/zmq.hpp 
    src/deps/cppzmq/zmq_addon.hpp
    src/deps/thread-pool/BS_thread_pool.hpp
    src/deps/simdjson-3.10.1/singleheader/simdjson.h
    src/deps/simdjson-3.10.1/singleheader/simdjson.cpp
    )

SOURCE_GROUP(Header_Files 	FILES ${HeaderFiles})
SOURCE_GROUP(Source_Files 	FILES ${SourceFiles})
SOURCE_GROUP(3rdParty 	    FILES ${3rdPartyFiles})

add_executable(zmq_task_dispatcher ${HeaderFiles} ${SourceFiles} ${3rdPartyFiles})

target_include_directories(zmq_task_dispatcher PRIVATE
    /usr/local/include 
    src
)

target_link_libraries(zmq_task_dispatcher PRIVATE
    zmq
    simdjson
    ${TRACY_LIBRARY}
    pthread
)

if(MSVC)
  SET(CompileOptions "")
else()
  SET(CompileOptions "-O3 -march=native -Wall -Wextra")
endif(MSVC)

target_compile_options(zmq_task_dispatcher PRIVATE ${CompileOptions})