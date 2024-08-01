cmake_minimum_required(VERSION 3.29)

project(
  UwRmq
  VERSION 0.1
  LANGUAGES CXX)
file(WRITE .name.project "${PROJECT_NAME}")

string(TOLOWER "${PROJECT_NAME}" BIN_NAME)
file(WRITE .name.bin "${BIN_NAME}")

option(ENABLE_DEBUG_FLAGS "Enable debug compilation flags" ON)

# Let the editor know where to find headers and deps
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Various root dirs for elements of the project
set(BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build")
set(PROJECT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(PROJECT_HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Target directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${BUILD_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${BUILD_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BUILD_DIR}/bin")

# Include libs and dirs
set(SOURCE_DIRS "${PROJECT_SOURCE_DIR}")
set(INCLUDE_DIRS "${PROJECT_HEADER_DIR}")

# Project source and target
set(SOURCES main.cpp)

list(TRANSFORM SOURCES PREPEND "${PROJECT_SOURCE_DIR}/")

# Binary to make from sources of this project
add_executable(${BIN_NAME} ${SOURCES})
set_property(TARGET "${BIN_NAME}" PROPERTY CXX_STANDARD 23)
target_include_directories(${BIN_NAME} PUBLIC "${PROJECT_HEADER_DIR}")

# Dependencies
find_package(Git REQUIRED)

find_package(fmt CONFIG REQUIRED)
target_link_libraries(${BIN_NAME} PRIVATE fmt::fmt-header-only)

find_package(amqpcpp CONFIG REQUIRED)
target_link_libraries(${BIN_NAME} PRIVATE amqpcpp)

find_package(libuv CONFIG REQUIRED)
target_link_libraries(
  ${BIN_NAME} PRIVATE $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>)

find_package(uvw CONFIG REQUIRED)
target_link_libraries(${BIN_NAME} PRIVATE uvw::uvw)

target_compile_options(${BIN_NAME} PRIVATE -Wall -Wextra)

set(_CMPL_FLAGS)
list(APPEND _CMPL_FLAGS -Wall)
list(APPEND _CMPL_FLAGS -Wcast-qual -Wcast-align)
list(APPEND _CMPL_FLAGS -Wdisabled-optimization)
list(APPEND _CMPL_FLAGS -Werror=return-type)
list(APPEND _CMPL_FLAGS -Wextra)
list(APPEND _CMPL_FLAGS -Wformat=2)
list(APPEND _CMPL_FLAGS -Winline)
list(APPEND _CMPL_FLAGS -Winvalid-pch)
# list(APPEND _CMPL_FLAGS -Wmissing-declarations)
list(APPEND _CMPL_FLAGS -Wmissing-include-dirs)
list(APPEND _CMPL_FLAGS -Wpacked)
list(APPEND _CMPL_FLAGS -Wpedantic)
list(APPEND _CMPL_FLAGS -Wredundant-decls)
list(APPEND _CMPL_FLAGS -Wstack-protector -fstack-protector)
list(APPEND _CMPL_FLAGS -Wstrict-overflow)
list(APPEND _CMPL_FLAGS -Wswitch -Wswitch-default)
list(APPEND _CMPL_FLAGS -Wundef)
list(APPEND _CMPL_FLAGS -Wuninitialized)
list(APPEND _CMPL_FLAGS -Wwrite-strings)

# list(APPEND _CMPL_FLAGS -Wstrict-aliasing -fstrict-aliasing) ↑ Disabled for
# libuv, see:
# https://github.com/libuv/libuv/blob/master/README.md#supported-platforms

list(APPEND _CMPL_FLAGS -Wno-missing-field-initializers)

# TODO: Implement compiler version checks
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # using Clang or AppleClang
  list(APPEND _CMPL_FLAGS -Warc-repeated-use-of-weak)
  list(APPEND _CMPL_FLAGS -Wbitfield-enum-conversion)
  list(APPEND _CMPL_FLAGS -Wc++11-compat-pedantic)
  list(APPEND _CMPL_FLAGS -Wclass-varargs)
  list(APPEND _CMPL_FLAGS -Wconditional-uninitialized)
  list(APPEND _CMPL_FLAGS -Werror=option-ignored)
  list(APPEND _CMPL_FLAGS -Wextra-semi)
  list(APPEND _CMPL_FLAGS -Wlogical-not-parentheses)
  list(APPEND _CMPL_FLAGS -Wthread-safety)

  list(APPEND _CMPL_FLAGS -Wno-pre-c++14-compat)
  list(APPEND _CMPL_FLAGS -Wno-pre-c++17-compat)
  list(APPEND _CMPL_FLAGS -Wno-pre-c++20-compat)
  list(APPEND _CMPL_FLAGS -Wno-pre-c++20-compat-pedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # using GCC
  list(APPEND _CMPL_FLAGS -Wlogical-op)
  list(APPEND _CMPL_FLAGS -Wduplicated-branches -Wduplicated-cond)
  list(APPEND _CMPL_FLAGS -Wstrict-null-sentinel)
  list(APPEND _CMPL_FLAGS -Wuseless-cast)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
  # using Intel C++
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # using Visual Studio C++ ... Why ?
endif()

set(CMAKE_BUILD_TYPE Debug)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(WARNING "Debug flags activated.")
  list(APPEND _CMPL_FLAGS -g)
  list(APPEND _CMPL_FLAGS -Werror)
  # list(APPEND _CMPL_FLAGS -Wfatal-errors)

  # list(APPEND _CMPL_FLAGS -fsanitize=address,leak,undefined)
endif()

set(_LINK_FLAGS)
list(APPEND _LINK_FLAGS ${_CMPL_FLAGS})

target_compile_options(${BIN_NAME} PRIVATE ${_CMPL_FLAGS})
target_link_options(${BIN_NAME} PRIVATE ${_LINK_FLAGS})
