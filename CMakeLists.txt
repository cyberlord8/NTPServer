# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(NTPServer C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(NTPServer
    src/main.cpp
    src/gps_uart.cpp
    src/gps_state.cpp
    src/led.cpp
    src/ui_console.cpp
    src/temp.cpp
    src/uptime.cpp
    src/wifi_cfg.cpp
    src/timebase.cpp
    src/ntp_server.cpp
    src/pps.cpp
    
)

target_include_directories(NTPServer PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(NTPServer
    pico_stdlib
    hardware_adc
    hardware_uart
    hardware_irq
    hardware_gpio
    hardware_spi
    hardware_i2c
    pico_cyw43_arch_lwip_threadsafe_background
)


pico_enable_stdio_usb(NTPServer 1)
pico_enable_stdio_uart(NTPServer 0)

pico_add_extra_outputs(NTPServer)

