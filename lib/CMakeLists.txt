add_library(lis3dh_i2c STATIC lis3dh_i2c.c)
add_library(elm327 STATIC elm327.c)
add_library(bt_setup STATIC bt_setup.c)
add_library(ds1302 STATIC ds1302.c)

target_link_libraries(lis3dh_i2c PRIVATE pico_stdlib hardware_i2c)
target_link_libraries(elm327 PRIVATE pico_stdlib)
target_link_libraries(bt_setup PRIVATE pico_stdlib pico_cyw43_arch_none pico_btstack_classic pico_btstack_cyw43)
target_link_libraries(ds1302 PRIVATE pico_stdlib)

target_include_directories(lis3dh_i2c PUBLIC .)
target_include_directories(elm327 PUBLIC .)
target_include_directories(bt_setup PUBLIC .)
target_include_directories(ds1302 PUBLIC .)
