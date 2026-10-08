cmake_minimum_required(VERSION 3.12)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(thing C CXX ASM)
pico_sdk_init()

add_executable(thing
        src/main.c
        src/tools.c
        src/handler.c
        src/flash.c
)

target_link_libraries(thing
        pico_stdlib
        hardware_spi
)

pico_add_extra_outputs(thing)

pico_enable_stdio_uart(thing 1)
pico_enable_stdio_usb(thing 0)
