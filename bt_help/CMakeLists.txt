#minimum cmake required version
cmake_minimum_required(VERSION 3.13)

#setting C/C++ standard libraries version
set(PICO_BOARD pico2_w)
set(BUILD_TYPE DEBUG)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

#including pico-sdk cmake build functions
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

#setting project name
project(bt_main)

#initializing pico-sdk
pico_sdk_init()


#adding main c file (bt_main.c) and hw_config.c for spi configuration needed for SPI sd card library
add_executable(bt_main
    bt_main.c
    bt_setup.c
)

add_subdirectory(../lib project_0/35/lib)

target_include_directories(bt_main PRIVATE
    ${CMAKE_CURRENT_LIST_DIR} # For btstack config
)

# pull in common dependencie
target_link_libraries(bt_main PRIVATE
    pico_stdlib
    pico_cyw43_arch_none
    pico_btstack_classic
    pico_btstack_cyw43
    elm327
)

# enable usb output, disable uart output
pico_enable_stdio_usb(bt_main 1)
pico_enable_stdio_uart(bt_main 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(bt_main)

