set(CORE "../../src")
idf_component_register(SRCS
    "core2_app.cpp" "board_link.cpp" "certabo_protocol.cpp"
    "${CORE}/types.cpp" "${CORE}/bitboard_compact.cpp" "${CORE}/position.cpp"
    "${CORE}/movegen.cpp" "${CORE}/weights.cpp" "${CORE}/eval.cpp"
    "${CORE}/botdata.cpp" "${CORE}/tt.cpp" "${CORE}/search.cpp"
    "${CORE}/adaptive.cpp" "${CORE}/uci.cpp"
    INCLUDE_DIRS "." "${CORE}"
    REQUIRES esp_driver_uart esp_system pthread esp_psram M5Unified esp-nimble-cpp)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE -O2 -Wno-deprecated-declarations)
