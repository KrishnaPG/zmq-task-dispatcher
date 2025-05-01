cmake_minimum_required(VERSION 3.10)
project(zmq_task_dispatcher CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

SET(ENABLE_TRACY OFF CACHE BOOL "Enable/Disable Tracy Profiler")

find_package(ZeroMQ REQUIRED)
if(ZeroMQ_FOUND)
else()
  message(FATAL_ERROR "ZeroMQ library not found")
endif()

if(ENABLE_TRACY)
    find_library(TRACY_LIBRARY NAMES tracy)
endif()

SET(HeaderFiles 
    src/shutdown.hpp
    src/tracer.hpp
    src/utils.hpp
    )

SET(SourceFiles 
    src/main.cpp
    src/shutdown.cpp
    )

SET(3rdPartyFiles 
    src/deps/cppzmq/zmq.hpp 
    src/deps/cppzmq/zmq_addon.hpp
    )

SOURCE_GROUP(Header_Files 	FILES ${HeaderFiles})
SOURCE_GROUP(Source_Files 	FILES ${SourceFiles})
SOURCE_GROUP(3rdParty 	    FILES ${3rdPartyFiles})

add_executable(zmq_task_dispatcher ${HeaderFiles} ${SourceFiles} ${3rdPartyFiles})

target_include_directories(zmq_task_dispatcher PRIVATE
    ${ZeroMQ_INCLUDE_DIRS}
    /usr/local/include 
    src
)

target_link_libraries(zmq_task_dispatcher PRIVATE
    ${ZeroMQ_LIBRARIES}
    ${TRACY_LIBRARY}
)

if(MSVC)
  SET(CompileOptions "")
else()
  SET(CompileOptions "-O3 -march=native -Wall -Wextra")
endif(MSVC)

target_compile_options(zmq_task_dispatcher PRIVATE ${CompileOptions})