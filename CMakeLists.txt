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

add_executable(virgin ${VIRGIN_SOURCES})
target_include_directories(virgin PRIVATE src)

# Keep the data files next to the binary for the tools / app packaging step.
add_custom_command(TARGET virgin POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_CURRENT_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:virgin>/data")

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
