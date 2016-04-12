project(pintool)
cmake_minimum_required(VERSION 2.8)

include(ExternalProject)

ExternalProject_Add (libsqlite
    SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/sqlite
    BUILD_IN_SOURCE 1
    URL https://www.sqlite.org/2016/sqlite-amalgamation-3120000.zip
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
    LOG_DOWNLOAD 1)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/sqlite)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/sqlite/sqlite3.c DEPENDS libsqlite)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/sqlite/sqlite3.c PROPERTIES COMPILE_FLAGS "-DSQLITE_THREADSAFE=0 -DSQLITE_DEFAULT_AUTOMATIC_INDEX=0 -DSQLITE_DEFAULT_SYNCHRONOUS=0")

ExternalProject_Add (pindownload
    SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/pin
    BUILD_IN_SOURCE 1
    URL http://software.intel.com/sites/landingpage/pintool/downloads/pin-2.14-71313-gcc.4.4.7-linux.tar.gz
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND ""
    LOG_DOWNLOAD 1)

ExternalProject_Add (libyamlcpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG release-0.5.3
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD 1
    LOG_CONFIGURE 1
    LOG_BUILD 1
    CMAKE_ARGS "-DCMAKE_CXX_FLAGS=-fPIC")


include_directories(${CMAKE_CURRENT_BINARY_DIR}/libyamlcpp-prefix/src/libyamlcpp/include)
link_directories(${CMAKE_CURRENT_BINARY_DIR}/libyamlcpp-prefix/src/libyamlcpp-build)

set(PIN_ROOT ${CMAKE_CURRENT_BINARY_DIR}/pin)

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

set(SRC_LIST_COMMON sqlwriter sqlite filter exception ${CMAKE_CURRENT_BINARY_DIR}/sqlite/sqlite3.c)
set(SRC_LIST_STATIC static ${SRC_LIST_COMMON})
set(SRC_LIST_DYNAMIC asm.h buffer dynamic manager entities threadmanager ${SRC_LIST_COMMON})

add_library(${PROJECT_NAME}_static SHARED ${SRC_LIST_STATIC})
add_library(${PROJECT_NAME}_dynamic SHARED ${SRC_LIST_DYNAMIC})

add_definitions(-DTARGET_IA32E -DHOST_IA32E -DTARGET_LINUX)
set(CMAKE_CXX_FLAGS "-g -fPIC -Wl,-Bsymbolic -std=c++11")

add_dependencies(${PROJECT_NAME}_static libsqlite)
target_link_libraries(${PROJECT_NAME}_static "pin" "pindwarf" "pinvm" "z" "sqlite3" "yaml-cpp" "dl" "rt")

add_dependencies(${PROJECT_NAME}_dynamic libsqlite)
target_link_libraries(${PROJECT_NAME}_dynamic "pin" "pindwarf" "pinvm" "z" "sqlite3" "yaml-cpp" "dl" "rt")
