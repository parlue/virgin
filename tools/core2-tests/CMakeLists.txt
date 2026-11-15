cmake_minimum_required(VERSION 3.20)
project(core2_game_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(ROOT "../..")
add_executable(core2_game_tests
  "${ROOT}/esp32_core2/main/game_tests.cpp"
  "${ROOT}/src/types.cpp" "${ROOT}/src/bitboard.cpp"
  "${ROOT}/src/position.cpp" "${ROOT}/src/movegen.cpp"
  "${ROOT}/src/weights.cpp" "${ROOT}/src/eval.cpp")
target_include_directories(core2_game_tests PRIVATE "${ROOT}/src" "${ROOT}/esp32_core2/main")
if(MSVC)
 target_compile_options(core2_game_tests PRIVATE /O2 /utf-8 /EHsc)
endif()

add_executable(certabo_core2_tests
  "${ROOT}/esp32_core2/main/certabo_core2_tests.cpp"
  "${ROOT}/esp32_core2/main/certabo_protocol.cpp")
target_include_directories(certabo_core2_tests PRIVATE "${ROOT}/esp32_core2/main")
if(MSVC)
 target_compile_options(certabo_core2_tests PRIVATE /O2 /utf-8 /EHsc)
endif()

add_executable(certabo_protocol_test "gateway/certabo_protocol_test.cpp" "${ROOT}/esp32_core2/main/certabo_protocol.cpp")
target_include_directories(certabo_protocol_test PRIVATE "${ROOT}/esp32_core2/main" "gateway")
target_compile_options(certabo_protocol_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)

add_executable(certabo_spectrum_test "gateway/certabo_spectrum_test.cpp" "${ROOT}/esp32_core2/main/certabo_protocol.cpp")
target_include_directories(certabo_spectrum_test PRIVATE "${ROOT}/esp32_core2/main" "gateway")
target_compile_options(certabo_spectrum_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)

add_executable(certabo_queen_layout_test "gateway/certabo_queen_layout_test.cpp" "${ROOT}/esp32_core2/main/certabo_protocol.cpp")
target_include_directories(certabo_queen_layout_test PRIVATE "${ROOT}/esp32_core2/main" "gateway")
target_compile_options(certabo_queen_layout_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)

add_executable(certabo_early_choice_test "gateway/certabo_early_choice_test.cpp" "${ROOT}/esp32_core2/main/certabo_protocol.cpp")
target_include_directories(certabo_early_choice_test PRIVATE "${ROOT}/esp32_core2/main" "gateway")
target_compile_options(certabo_early_choice_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)

add_executable(certabo_startup_regression_test "gateway/certabo_startup_regression_test.cpp" "${ROOT}/esp32_core2/main/certabo_protocol.cpp")
target_include_directories(certabo_startup_regression_test PRIVATE "${ROOT}/esp32_core2/main" "gateway")
target_compile_options(certabo_startup_regression_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>)
