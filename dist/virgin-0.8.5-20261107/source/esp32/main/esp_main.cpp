#include "bitboard.h"
#include "embedded_config.h"
#include "position.h"
#include "search.h"
#include "tt.h"
#include "uci.h"
#include "weights.h"
#ifdef VIRGIN_CORE2
#include "driver/uart.h"
#include "driver/uart_vfs.h"
#else
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#endif
#include "esp_heap_caps.h"
#include "esp_pthread.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#ifdef VIRGIN_USE_PSRAM
#include "esp_psram.h"
#endif
#include <cstdio>
#include <iostream>
#include <exception>
#include <stdexcept>

static void yieldSearch() { vTaskDelay(1); }
extern "C" void app_main() {
#ifdef VIRGIN_CORE2
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 4096, 4096, 0, nullptr, 0));
    uart_vfs_dev_use_driver(UART_NUM_0);
    uart_vfs_dev_port_set_rx_line_endings(UART_NUM_0, ESP_LINE_ENDINGS_CRLF);
    uart_vfs_dev_port_set_tx_line_endings(UART_NUM_0, ESP_LINE_ENDINGS_LF);
#else
    usb_serial_jtag_driver_config_t usb{};
    usb.rx_buffer_size = 8192; usb.tx_buffer_size = 8192;
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
    usb_serial_jtag_vfs_use_driver();
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_CRLF);
    usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_LF);
#endif
    setvbuf(stdin, nullptr, _IONBF, 0);
    setvbuf(stdout, nullptr, _IONBF, 0);
    try {
        esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
        cfg.prio = 4; cfg.pin_to_core = 1; cfg.inherit_cfg = true; cfg.thread_name = "virgin_search";
#ifdef VIRGIN_USE_PSRAM
        if (!esp_psram_is_initialized() || esp_psram_get_size() < 2 * 1024 * 1024)
            throw std::runtime_error("PSRAM profile needs at least 2 MiB PSRAM");
        heap_caps_malloc_extmem_enable(4096);
#ifdef VIRGIN_CORE2
        // Classic ESP32: keep the search stack internal; reserve PSRAM for
        // the hash. ROM/cache restrictions differ from the ESP32-S3.
        cfg.stack_size = 96 * 1024;
        cfg.stack_alloc_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
#else
        cfg.stack_size = 256 * 1024;
        cfg.stack_alloc_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
#endif
#else
        cfg.stack_size = 96 * 1024;
        cfg.stack_alloc_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
#endif
        ESP_ERROR_CHECK(esp_pthread_set_cfg(&cfg));
        init_bitboards(); init_zobrist(); init_weights(); initSearch();
        TT.resizeKiB(VIRGIN_EMBEDDED_HASH_KIB);
        setSearchPoll(yieldSearch);
        char name[] = "virgin"; char* argv[] = {name, nullptr};
        uciInit(1, argv);
        std::cout << 
#ifdef VIRGIN_CORE2
                     "info string Virgin M5Stack Core2 ready: hash "
#else
                     "info string Virgin ESP32-S3 ready: hash "
#endif
 << VIRGIN_EMBEDDED_HASH_KIB
                  << " KiB, one search worker, stack limit " << VIRGIN_EMBEDDED_MAX_PLY
                  << " plies, free internal " << heap_caps_get_free_size(MALLOC_CAP_INTERNAL) << std::endl;
        uciLoop(1, argv);
    } catch (const std::exception& error) {
        std::cout << "info string firmware error: " << error.what() << std::endl;
    }
    // quit returns to a safe stopped state; reset starts a fresh UCI session.
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}
