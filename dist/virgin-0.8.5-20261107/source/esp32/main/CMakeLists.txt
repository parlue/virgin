set(CORE "../../src")
idf_component_register(SRCS
    "esp_main.cpp"
    "${CORE}/types.cpp" "${CORE}/bitboard_compact.cpp" "${CORE}/position.cpp"
    "${CORE}/movegen.cpp" "${CORE}/weights.cpp" "${CORE}/eval.cpp"
    "${CORE}/botdata.cpp" "${CORE}/tt.cpp" "${CORE}/search.cpp"
    "${CORE}/adaptive.cpp" "${CORE}/uci.cpp"
    INCLUDE_DIRS "${CORE}"
    REQUIRES esp_driver_usb_serial_jtag esp_system pthread esp_psram vfs)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE -O2 -Wno-deprecated-declarations)
