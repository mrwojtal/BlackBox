#ifndef DS1302_H
#define DS1302_H

#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

#define TIME_RESPONSE_BUFFER_SIZE 64

//read data registers are write registers +1

//write data registers
#define SECONDS_W 0x80
#define MINUTES_W 0x82
#define HOUR_W 0x84
#define DATE_W 0x86
#define MONTH_W 0x88
#define DAY_W 0x8A
#define YEAR_W 0x8C
#define WP_W 0x8E

typedef struct DS1302Instance {
    uint8_t clk;
    uint8_t data;
    uint8_t rst;
} DS1302Instance;

void ds1302_init(DS1302Instance* ds1302);

void ds1302_set_time(DS1302Instance* ds1302, uint8_t* date);

void ds1302_get_time(DS1302Instance* ds1302, char* buffer, uint8_t buffer_size);

void ds1302_get_time_hm(DS1302Instance* ds1302, char* buffer, uint8_t buffer_size);

void ds1302_get_date(DS1302Instance* ds1302, char* buffer, uint8_t buffer_size);

void ds1302_get_date_and_time(DS1302Instance* ds1302, char* buffer, uint8_t buffer_size);

#endif