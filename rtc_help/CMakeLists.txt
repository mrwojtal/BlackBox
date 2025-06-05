#minimum cmake required version
cmake_minimum_required(VERSION 3.13)

#setting C/C++ standard libraries version
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

#setting pico board and build type
set(PICO_BOARD pico2_w)
set(BUILD_TYPE DEBUG)

#including pico-sdk cmake build functions
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

#setting project name
project(rtc)

pico_sdk_init()

add_executable(rtc main.c ds1302.c)

target_link_libraries(rtc PRIVATE pico_stdlib)

# enable usb output, disable uart output
pico_enable_stdio_usb(rtc 1)
pico_enable_stdio_uart(rtc 0)

pico_add_extra_outputs(rtc)