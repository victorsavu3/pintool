project(pintool)
cmake_minimum_required(VERSION 2.8)

include(ExternalProject)

ExternalProject_Add (libsqlite
    SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/sqlite
    BUILD_IN_SOURCE 1
    URL https://www.sqlite.org/2016/sqlite-autoconf-3110000.tar.gz
    CONFIGURE_COMMAND ${CMAKE_CURRENT_BINARY_DIR}/sqlite/configure
    BUILD_COMMAND ${MAKE}
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
    LOG_DOWNLOAD 1
    LOG_CONFIGURE 1
    LOG_BUILD 1)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/sqlite)
link_directories(${CMAKE_CURRENT_BINARY_DIR}/sqlite/.libs)

ExternalProject_Add (libyamlcpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG release-0.5.3
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD 1
    LOG_CONFIGURE 1
    LOG_BUILD 1
    CMAKE_ARGS "-DBUILD_SHARED_LIBS=ON")


include_directories(${CMAKE_CURRENT_BINARY_DIR}/libyamlcpp-prefix/src/libyamlcpp/include)
link_directories(${CMAKE_CURRENT_BINARY_DIR}/libyamlcpp-prefix/src/libyamlcpp-build)

find_path(PIN_ROOT
  NAMES source/include/pin/pin.H
  PATHS $ENV{PIN_ROOT}
  DOC "Pin's base directory"
)
if(NOT PIN_ROOT)
  message(FATAL_ERROR
    "\nPin not found!\n"
    "Please set the environment variable PIN_ROOT to the base directory"
    " of the Pin.\n Please make sure the same pin installation is in USER or SYSTEM PATH as well"
  )
endif(NOT PIN_ROOT)
message(STATUS "PIN_ROOT: ${PIN_ROOT}")

message(STATUS ${CMAKE_C_COMPILER_VERSION} )

set(PIN_INCLUDE_DIRS
  ${PIN_ROOT}/extras/xed-intel64/include
  ${PIN_ROOT}/source/include/pin
  ${PIN_ROOT}/source/include/pin/gen
  ${PIN_ROOT}/extras/components/include
  ${PIN_ROOT}/source/tools/InstLib
)
set(PIN_LIBRARY_DIRS
  ${PIN_ROOT}/extras/components/lib/intel64
  ${PIN_ROOT}/extras/xed-intel64/lib
  ${PIN_ROOT}/intel64/lib
  ${PIN_ROOT}/intel64/lib-ext
)

include_directories(${PIN_INCLUDE_DIRS})
link_directories (${PIN_LIBRARY_DIRS})

set(SRC_LIST_COMMON sqlwriter sqlite filter)
set(SRC_LIST_STATIC static ${SRC_LIST_COMMON})
set(SRC_LIST_DYNAMIC dynamic manager entities ${SRC_LIST_COMMON})

add_library(${PROJECT_NAME}_static SHARED ${SRC_LIST_STATIC})
add_library(${PROJECT_NAME}_dynamic SHARED ${SRC_LIST_DYNAMIC})

add_definitions(-DTARGET_IA32E -DHOST_IA32E -DTARGET_LINUX)
set(CMAKE_CXX_FLAGS "-g -fPIC -Wl,-Bsymbolic -std=c++11")

add_dependencies(${PROJECT_NAME}_static libsqlite)
target_link_libraries(${PROJECT_NAME}_static "pin" "pindwarf" "pinvm" "z" "sqlite3" "yaml-cpp" "dl" "rt")

add_dependencies(${PROJECT_NAME}_dynamic libsqlite)
target_link_libraries(${PROJECT_NAME}_dynamic "pin" "pindwarf" "pinvm" "z" "sqlite3" "yaml-cpp" "dl" "rt")
