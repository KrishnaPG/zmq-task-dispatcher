cmake_minimum_required(VERSION 3.10)
project(zmq_task_dispatcher CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)
# add_definitions(-DENABLE_TRACY)

find_package(ZeroMQ REQUIRED)
#find_package(simdjson REQUIRED)
#find_library(TRACY_LIBRARY NAMES tracy)

add_executable(zmq_task_dispatcher
    src/main.cpp
    src/zmq_server.cpp
    src/message_dispatcher.cpp
    src/jsonrpc_handler.cpp
    src/deps/simdjson-3.10.1/singleheader/simdjson.cpp
)

target_include_directories(zmq_task_dispatcher PRIVATE
    /usr/local/include src
)

target_link_libraries(zmq_task_dispatcher PRIVATE
    zmq
    simdjson
    ${TRACY_LIBRARY}
    pthread
)

target_compile_options(zmq_task_dispatcher PRIVATE -O3 -march=native -Wall -Wextra)