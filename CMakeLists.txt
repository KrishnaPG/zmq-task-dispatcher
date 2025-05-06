cmake_minimum_required(VERSION 4.0)

if(WIN32)
set(ENABLE_VCPKG ON CACHE BOOL "Use VCPkg for package finding")
  if(ENABLE_VCPKG)
    set(VCPKG_ROOT_DIR $ENV{VCPKG_ROOT} CACHE PATH "VCPkg Root Folder Path (where vcpkg.exe is located)")
    if("${VCPKG_ROOT_DIR}" STREQUAL "")
      message(FATAL_ERROR "\nVCPKG_ROOT is not found. \nChoose the VCPKG_ROOT_DIR path to proceed, or turn off the ENABLE_VCPKG option to manually discover the packages.")
    endif()
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT_DIR}/scripts/buildsystems/vcpkg.cmake")
  endif()
endif(WIN32)

project(zmq-task-dispatcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

SET(ENABLE_TRACY OFF CACHE BOOL "Enable/Disable Tracy Profiler")

if(ENABLE_TRACY)
  find_package(Tracy REQUIRED)

  set(Tracy_LIBRARIES Tracy::TracyClient)
  # Ensure debug info and frame pointers for accurate profiling
  # set(TracyCompileOptions -g -fno-omit-frame-pointer -rdynamic)
endif()

# Find ZeroMQ
if(WIN32)
  find_package(ZeroMQ REQUIRED)
  set(ZeroMQ_LIBRARIES ${ZeroMQ_LIBRARIES} libzmq)
  set(ZeroMQ_INCLUDE_DIRS ${ZeroMQ_INCLUDE_DIRS} ${ZeroMQ_INCLUDE_DIR})
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(ZeroMQ REQUIRED libzmq)
endif(WIN32)

# Find C++ header for ZeroMQ
find_package(cppzmq REQUIRED)

SET(HeaderFiles 
    src/messages.hpp
    src/shutdown.hpp
    src/tracer.hpp
    src/utils.hpp
    )

SET(SourceFiles 
    src/main.cpp
    src/messages.cpp
    src/shutdown.cpp
    )

SET(3rdPartyFiles 
    src/deps/thread-pool/BS_thread_pool.hpp
    )

SOURCE_GROUP(Header_Files 	FILES ${HeaderFiles})
SOURCE_GROUP(Source_Files 	FILES ${SourceFiles})
SOURCE_GROUP(3rdParty 	    FILES ${3rdPartyFiles})

add_executable(zmq-task-dispatcher ${HeaderFiles} ${SourceFiles} ${3rdPartyFiles})

target_include_directories(zmq-task-dispatcher PRIVATE
    ${ZeroMQ_INCLUDE_DIRS}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_compile_options(zmq-task-dispatcher PRIVATE ${TracyCompileOptions})

target_link_libraries(zmq-task-dispatcher PRIVATE
    ${ZeroMQ_LIBRARIES}
    ${Tracy_LIBRARIES}
)

