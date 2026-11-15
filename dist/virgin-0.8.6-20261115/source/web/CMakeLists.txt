cmake_minimum_required(VERSION 3.20)
project(virgin_web LANGUAGES CXX)
if(NOT EMSCRIPTEN)
  message(FATAL_ERROR "Use emcmake cmake to configure the WebAssembly target")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CORE "${CMAKE_CURRENT_LIST_DIR}/../src")
add_executable(virgin
  "${CORE}/types.cpp" "${CORE}/bitboard.cpp" "${CORE}/position.cpp"
  "${CORE}/movegen.cpp" "${CORE}/eval.cpp" "${CORE}/weights.cpp"
  "${CORE}/botdata.cpp" "${CORE}/tt.cpp" "${CORE}/adaptive.cpp"
  "${CORE}/search.cpp" "${CORE}/uci.cpp" "${CORE}/virgin_native.cpp")
target_include_directories(virgin PRIVATE "${CORE}")
target_compile_definitions(virgin PRIVATE VIRGIN_LIBRARY=1 VIRGIN_WASM=1 NDEBUG)
target_compile_options(virgin PRIVATE -O3 -flto -pthread -fno-rtti -fexceptions)
target_link_options(virgin PRIVATE -O3 -flto -pthread -fexceptions --no-entry
  "-sMODULARIZE=1" "-sEXPORT_ES6=1" "-sEXPORT_NAME=createVirgin" "-sENVIRONMENT=web,worker"
  "-sPTHREAD_POOL_SIZE=5" "-sPTHREAD_POOL_SIZE_STRICT=2"
  "-sSTACK_SIZE=2097152" "-sDEFAULT_PTHREAD_STACK_SIZE=2097152"
  "-sINITIAL_MEMORY=134217728" "-sMAXIMUM_MEMORY=536870912" "-sALLOW_MEMORY_GROWTH=1"
  "-sEXPORTED_FUNCTIONS=['_virgin_create','_virgin_send','_virgin_read','_virgin_destroy','_malloc','_free']"
  "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','stringToUTF8','lengthBytesUTF8','PThread']"
  "-sASSERTIONS=1")
set_target_properties(virgin PROPERTIES SUFFIX ".js")
