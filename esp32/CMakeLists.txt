cmake_minimum_required(VERSION 3.16)
# UCI firmware: build only main and its declared recursive dependencies.
set(COMPONENTS main __pio_env)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(virgin_esp32s3)
