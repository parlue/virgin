cmake_minimum_required(VERSION 3.20)
project(virgin LANGUAGES CXX)

# Virgin - UCI chess engine (bot-oriented, style driven)
# Targets: Windows x64 (MSVC) and Android (NDK, arm64-v8a / armeabi-v7a / x86_64)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(VIRGIN_LIBRARY "Build the in-process shared library with C API / Android JNI" OFF)

option(VIRGIN_LTO "Enable link time optimization" ON)
option(VIRGIN_STATIC "Link the Android executable fully static (no dependencies)" OFF)

set(VIRGIN_SOURCES
  src/main.cpp
  src/types.cpp
  src/bitboard.cpp
  src/position.cpp
  src/movegen.cpp
  src/eval.cpp
  src/weights.cpp
  src/botdata.cpp
  src/match.cpp
  src/tt.cpp
  src/adaptive.cpp
  src/search.cpp
  src/tune.cpp
  src/selftest.cpp
  src/uci.cpp
)

option(VIRGIN_EMBEDDED "Host verification of the compact ESP32 backend" OFF)
set(VIRGIN_EMBEDDED_HASH_KIB 32 CACHE STRING "Compact hash in KiB")
set(VIRGIN_EMBEDDED_MAX_PLY 24 CACHE STRING "Compact search stack limit")
if(VIRGIN_EMBEDDED)
  list(REMOVE_ITEM VIRGIN_SOURCES src/bitboard.cpp)
  list(APPEND VIRGIN_SOURCES src/bitboard_compact.cpp)
endif()
if(VIRGIN_LIBRARY)
  if(VIRGIN_STATIC OR VIRGIN_EMBEDDED)
    message(FATAL_ERROR "VIRGIN_LIBRARY requires the normal shared-library backend")
  endif()
  list(REMOVE_ITEM VIRGIN_SOURCES src/main.cpp src/match.cpp src/tune.cpp src/selftest.cpp)
  list(APPEND VIRGIN_SOURCES src/virgin_native.cpp)
  if(ANDROID)
    list(APPEND VIRGIN_SOURCES src/virgin_jni.cpp)
  endif()
  add_library(virgin SHARED ${VIRGIN_SOURCES})
  target_compile_definitions(virgin PRIVATE VIRGIN_LIBRARY=1)
  set_target_properties(virgin PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
else()
  add_executable(virgin ${VIRGIN_SOURCES})
endif()
if(VIRGIN_EMBEDDED)
  target_compile_definitions(virgin PRIVATE VIRGIN_EMBEDDED=1
    VIRGIN_EMBEDDED_HASH_KIB=${VIRGIN_EMBEDDED_HASH_KIB}
    VIRGIN_EMBEDDED_MAX_PLY=${VIRGIN_EMBEDDED_MAX_PLY})
endif()
target_include_directories(virgin PRIVATE src)

# Keep the data files next to the binary for the tools / app packaging step.
if(NOT VIRGIN_LIBRARY)
add_custom_command(TARGET virgin POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:virgin>/data")
endif()

if(MSVC)
  target_compile_options(virgin PRIVATE /W4 /O2 /fp:fast /EHsc /DNDEBUG /utf-8)
  if(VIRGIN_LTO)
    target_compile_options(virgin PRIVATE /GL)
    target_link_options(virgin PRIVATE /LTCG)
  endif()
else()
  target_compile_options(virgin PRIVATE -Wall -Wextra -O3 -DNDEBUG -fno-rtti)
  if(ANDROID)
    # Small, stripped, no external dependencies. Fully static linking is
    # optional: PIE plus the Android system libraries is the default because it
    # is the most compatible form for chess GUIs.
    target_compile_options(virgin PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(virgin PRIVATE -Wl,--gc-sections -Wl,-s)
    if(VIRGIN_LIBRARY)
      target_link_options(virgin PRIVATE -Wl,-z,max-page-size=16384 -Wl,-Bsymbolic -Wl,--exclude-libs,ALL)
    endif()
    if(VIRGIN_STATIC)
      target_link_options(virgin PRIVATE -static)
    endif()
  endif()
  if(VIRGIN_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT VIRGIN_IPO_OK OUTPUT VIRGIN_IPO_MSG)
    if(VIRGIN_IPO_OK)
      set_property(TARGET virgin PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
      message(STATUS "IPO/LTO not supported: ${VIRGIN_IPO_MSG}")
    endif()
  endif()
endif()

set_target_properties(virgin PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}")
