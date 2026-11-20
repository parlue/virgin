cmake_minimum_required(VERSION 3.16)
# UCI firmware: build only main and its declared recursive dependencies.
set(COMPONENTS main __pio_env)
set(PROJECT_VER "0.8.7")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(virgin_esp32s3)
