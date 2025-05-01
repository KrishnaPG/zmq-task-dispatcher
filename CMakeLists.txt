cmake_minimum_required(VERSION 3.10)
project(zmq-task-dispatcher CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

SET(ENABLE_TRACY OFF CACHE BOOL "Enable/Disable Tracy Profiler")

if(WIN32)
  find_package(ZeroMQ REQUIRED)
  if(ZeroMQ_FOUND)
    message(STATUS "Found ZeroMQ: ${ZeroMQ_INCLUDE_DIRS}, ${ZeroMQ_LIBRARIES}")
  else()
    message(FATAL_ERROR "ZeroMQ library not found")
  endif(ZeroMQ_FOUND)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(ZeroMQ REQUIRED libzmq)
endif(WIN32)

if(ENABLE_TRACY)
    find_library(TRACY_LIBRARY NAMES tracy)
endif()

SET(HeaderFiles 
    src/shutdown.hpp
    src/tracer.hpp
    src/utils.hpp
    src/worker-pool.hpp
    )

SET(SourceFiles 
    src/main.cpp
    src/shutdown.cpp
    src/worker-pool.cpp
    )

SET(3rdPartyFiles 
    src/deps/cppzmq/zmq.hpp 
    src/deps/cppzmq/zmq_addon.hpp
    )

SOURCE_GROUP(Header_Files 	FILES ${HeaderFiles})
SOURCE_GROUP(Source_Files 	FILES ${SourceFiles})
SOURCE_GROUP(3rdParty 	    FILES ${3rdPartyFiles})

add_executable(zmq-task-dispatcher ${HeaderFiles} ${SourceFiles} ${3rdPartyFiles})

target_include_directories(zmq-task-dispatcher PRIVATE
    ${ZeroMQ_INCLUDE_DIRS}
    /usr/local/include 
    src
)

target_link_libraries(zmq-task-dispatcher PRIVATE
    ${ZeroMQ_LIBRARIES}
    ${TRACY_LIBRARY}
)

target_compile_options(zmq-task-dispatcher PRIVATE ${CompileOptions})