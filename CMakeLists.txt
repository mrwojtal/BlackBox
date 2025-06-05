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
project(black_box)

#initializing pico-sdk
pico_sdk_init()

#adding no-OS-FatFS-SD_SDIO_SPI-RPi-Pico library
add_subdirectory(../../no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src build)

#adding main c file (black_box.c) and hw_config.c for spi configuration needed for SPI sd card library
add_executable(black_box
    black_box.c
    hw_config.c
    )

#adding lis3dh_i2c, elm327, bt_setup library
add_subdirectory(lib)

target_include_directories(black_box PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/lib # For btstack config
)

# pull in common dependencie
target_link_libraries(black_box PRIVATE
    pico_stdlib 
    hardware_watchdog
    hardware_i2c
    lis3dh_i2c
    pico_cyw43_arch_none
    pico_btstack_classic
    pico_btstack_cyw43
    bt_setup
    elm327
    ds1302
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)

# enable usb output, disable uart output
pico_enable_stdio_usb(black_box 1)
pico_enable_stdio_uart(black_box 0)

# binary info, can be read by picotool
#not working, need debugging
#pico_set_program_name(black_box, "black_box")
#pico_set_program_description(black_box, "Application used for logging data from car OBD-II interface.")
#pico_set_program_version(black_box, "0.1")

# create map/bin/hex file etc.
pico_add_extra_outputs(black_box)

