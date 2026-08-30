cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico CACHE STRING "Board type")
set(PICO_SDK_PATH "C:/repos/Pico-Examples/pico-sdk" CACHE PATH "Pico SDK path")
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(squib_box_dmx_2 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

add_library(picodmx_input INTERFACE)
target_sources(picodmx_input INTERFACE lib/DmxInput.cpp)
target_include_directories(picodmx_input INTERFACE ${CMAKE_CURRENT_LIST_DIR}/lib)
pico_generate_pio_header(picodmx_input ${CMAKE_CURRENT_LIST_DIR}/lib/DmxInput.pio)

add_executable(squib_box_dmx_2 main.cpp)
target_link_libraries(squib_box_dmx_2
    pico_stdlib
    hardware_dma
    hardware_pio
    hardware_pwm
    picodmx_input
)

target_compile_options(squib_box_dmx_2 PRIVATE -Wall -Wextra -Wpedantic)
pico_enable_stdio_usb(squib_box_dmx_2 0)
pico_enable_stdio_uart(squib_box_dmx_2 0)
pico_add_extra_outputs(squib_box_dmx_2)
