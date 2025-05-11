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

# Add BS::thread_pool (a header-only library)
include(FetchContent)
FetchContent_Declare(
    bs_thread_pool
    GIT_REPOSITORY https://github.com/bshoshany/thread-pool.git
    GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(bs_thread_pool)
set(BS_THREAD_POOL_INCLUDE_DIRS ${bs_thread_pool_SOURCE_DIR}/include/)

# Add FMT
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 11.2.0
)
FetchContent_MakeAvailable(fmt)

# Add MiMalloc
FetchContent_Declare(
    mimalloc 
    GIT_REPOSITORY https://github.com/microsoft/mimalloc.git
    GIT_TAG v3.0.3
)
FetchContent_MakeAvailable(mimalloc)
set(MIMALLOC_INCLUDE_DIRS ${mimalloc_SOURCE_DIR}/include/)

SET(HeaderFiles 
    src/custom-memory.hpp
    src/headers.hpp
    src/lockfree_object_pool.hpp
    src/messages.hpp
    src/methods.hpp
    src/mpsc_queue.hpp
    src/shutdown.hpp
    src/tracer.hpp
    src/utils.hpp
    )

SET(SourceFiles 
    src/main.cpp
    src/messages.cpp
    src/methods.cpp
    src/shutdown.cpp
    )

SET(3rdPartyFiles 
    )

SOURCE_GROUP(Header_Files 	FILES ${HeaderFiles})
SOURCE_GROUP(Source_Files 	FILES ${SourceFiles})
SOURCE_GROUP(3rdParty 	    FILES ${3rdPartyFiles})

add_executable(zmq-task-dispatcher ${HeaderFiles} ${SourceFiles} ${3rdPartyFiles})

target_include_directories(zmq-task-dispatcher PRIVATE
    ${ZeroMQ_INCLUDE_DIRS}
    ${BS_THREAD_POOL_INCLUDE_DIRS}
    ${MIMALLOC_INCLUDE_DIRS}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include> # these dirs are only used when building inside the build tree
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> # these dirs are only used when linking against a prebuilt version of your package
)

target_compile_options(zmq-task-dispatcher PRIVATE ${TracyCompileOptions})

target_link_libraries(zmq-task-dispatcher PRIVATE
    ${ZeroMQ_LIBRARIES}
    ${Tracy_LIBRARIES}  
    fmt
    mimalloc-static 
)

