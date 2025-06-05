#include "ds1302.h"

void ds1302_init(DS1302Instance* ds1302) {    
    //initialize clk
    gpio_init(ds1302->clk);
    gpio_set_dir(ds1302->clk, GPIO_OUT);

    //initialize rst 
    gpio_init(ds1302->rst);
    gpio_set_dir(ds1302->rst, GPIO_OUT);
    
    //initialize data
    gpio_init(ds1302->data);
    gpio_set_dir(ds1302->data, GPIO_OUT);
}

static uint8_t _dec_to_hex(uint8_t byte) {
    return ((byte/10) * 16 + (byte%10));
}

static uint8_t _hex_to_dec(uint8_t byte) {
    return ((byte/16) * 10 + (byte%16));
}

static void _write_byte(DS1302Instance* ds1302, uint8_t byte) {
    gpio_set_dir(ds1302->data, GPIO_OUT);
    for(uint8_t i = 0; i < 8; i++) {
        gpio_put(ds1302->data, (byte>>i)&1);
        sleep_us(1);
        gpio_put(ds1302->clk, 1);
        sleep_us(1);
        gpio_put(ds1302->clk, 0);
    }
}

static uint8_t _read_byte(DS1302Instance* ds1302) {
    uint8_t response = 0;
    gpio_set_dir(ds1302->data, GPIO_IN);
    for(uint8_t i = 0; i < 8; i++) {
        response |= (gpio_get(ds1302->data)<<i);
        sleep_us(1);
        gpio_put(ds1302->clk, 1);
        sleep_us(1);
        gpio_put(ds1302->clk, 0);
    }
    return response;
}

static void _set_reg(DS1302Instance* ds1302, uint8_t reg, uint8_t data) {
    gpio_put(ds1302->rst, 1);
    _write_byte(ds1302, reg);
    _write_byte(ds1302, data);
    gpio_put(ds1302->rst, 0);
}

static uint8_t _get_reg(DS1302Instance* ds1302, uint8_t reg) {
    uint8_t response = 0;
    gpio_put(ds1302->rst, 1);
    _write_byte(ds1302, reg);
    response = _read_byte(ds1302);
    gpio_put(ds1302->rst, 0);
    return response;
}

static void _ds1302_disable_wp(DS1302Instance* ds1302) {
    _set_reg(ds1302, WP_W, 0x00);
}

static void _ds1302_enable_wp(DS1302Instance* ds1302) {
    _set_reg(ds1302, WP_W, 0x80);
}

static void _ds1302_set_seconds(DS1302Instance* ds1302, uint8_t seconds) {
    uint8_t raw = _dec_to_hex(seconds % 60);
    raw &= 0x7F; // wyzeruj CH
    _set_reg(ds1302, SECONDS_W, raw);
}

static void _ds1302_set_minutes(DS1302Instance* ds1302, uint8_t minutes) {
    _set_reg(ds1302, MINUTES_W, _dec_to_hex(minutes%60));
}

static void _ds1302_set_hour(DS1302Instance* ds1302, uint8_t hour) {
    _set_reg(ds1302, HOUR_W, _dec_to_hex(hour%24));
}

static void _ds1302_set_date(DS1302Instance* ds1302, uint8_t date) {
    _set_reg(ds1302, DATE_W, _dec_to_hex(date%32));
}

static void _ds1302_set_day(DS1302Instance* ds1302, uint8_t day) {
    _set_reg(ds1302, DAY_W, _dec_to_hex(day%8));
}

static void _ds1302_set_month(DS1302Instance* ds1302, uint8_t month) {
    _set_reg(ds1302, MONTH_W, _dec_to_hex(month%13));
}

static void _ds1302_set_year(DS1302Instance* ds1302, uint8_t year) {
    _set_reg(ds1302, YEAR_W, _dec_to_hex(year));
}

static uint8_t _ds1302_get_seconds(DS1302Instance* ds1302) {
	uint8_t response = 0;
    response = _hex_to_dec(_get_reg(ds1302, SECONDS_W+1))%60;
	return response;
}

static uint8_t _ds1302_get_minutes(DS1302Instance* ds1302) {
	uint8_t response = 0;
    response = _hex_to_dec(_get_reg(ds1302, MINUTES_W+1));
	return response;
}

static uint8_t _ds1302_get_hour(DS1302Instance* ds1302) {
	uint8_t response = 0;
    response = _hex_to_dec(_get_reg(ds1302, HOUR_W+1));
	return response;
}

static uint8_t _ds1302_get_date(DS1302Instance* ds1302) {
	uint8_t response = 0;
    response = _hex_to_dec(_get_reg(ds1302, DATE_W+1));
	return response;
}

static uint8_t _ds1302_get_day(DS1302Instance* ds1302) {
	uint8_t response = 0;
    response = _hex_to_dec(_get_reg(ds1302, DAY_W+1));
	return response;
}

static uint8_t _ds1302_get_month(DS1302Instance* ds1302) {
	uint8_t response = 0;
    response = _hex_to_dec(_get_reg(ds1302, MONTH_W+1));
	return response;
}

static uint8_t _ds1302_get_year(DS1302Instance* ds1302) {
	uint8_t response = 0;
    response = _hex_to_dec(_get_reg(ds1302, YEAR_W+1));
	return response;
}

/// @brief provide DS1302Instance struct and date as an array 
/// @param ds1302 struct for ds1302 instance
/// @param time time to set in format HH;MM;SS;DAY;MONTH;YEAR
void ds1302_set_time(DS1302Instance* ds1302, uint8_t* date) {
    _ds1302_disable_wp(ds1302);
    _ds1302_set_year(ds1302, date[0]);
    _ds1302_set_month(ds1302, date[1]);
    _ds1302_set_date(ds1302, date[2]);
    _ds1302_set_day(ds1302, date[3]);
    _ds1302_set_hour(ds1302, date[4]);
    _ds1302_set_minutes(ds1302, date[5]);
    _ds1302_set_seconds(ds1302, date[6]);
}

void ds1302_get_time(DS1302Instance* ds1302, char* buffer, uint8_t buffer_size) {
    
    uint8_t seconds = _ds1302_get_seconds(ds1302);
    uint8_t minutes = _ds1302_get_minutes(ds1302);
    uint8_t hour = _ds1302_get_hour(ds1302);
    if (hour < 10 && minutes < 10 && seconds < 10){
        snprintf(buffer, buffer_size, "0%d;0%d;0%d", hour, minutes, seconds);
    }
    else if (hour >= 10 && minutes >= 10 && seconds >= 10) {
        snprintf(buffer, buffer_size, "%d;%d;%d", hour, minutes, seconds);
    }
    else if (hour < 10 && minutes >= 10 && seconds >= 10) {
        snprintf(buffer, buffer_size, "0%d;%d;%d", hour, minutes, seconds);
    }
    else if (hour >= 10 && minutes < 10 && seconds >= 10) {
        snprintf(buffer, buffer_size, "%d;0%d;%d", hour, minutes, seconds);
    }
    else if (hour >= 10 && minutes >= 10 && seconds < 10) {
        snprintf(buffer, buffer_size, "%d;%d;0%d", hour, minutes, seconds);
    }
    else if (hour < 10 && minutes < 10 && seconds >= 10) {
        snprintf(buffer, buffer_size, "0%d;0%d;%d", hour, minutes, seconds);
    }
    else if (hour >= 10 && minutes < 10 && seconds < 10) {
        snprintf(buffer, buffer_size, "%d;0%d;0%d", hour, minutes, seconds);
    }
    else if (hour < 10 && minutes >= 10 && seconds < 10) {
        snprintf(buffer, buffer_size, "0%d;%d;0%d", hour, minutes, seconds);
    }
}

void ds1302_get_date(DS1302Instance* ds1302, char* buffer, uint8_t buffer_size) {
    uint16_t year = 2000+_ds1302_get_year(ds1302);
    uint8_t month = _ds1302_get_month(ds1302);
    uint8_t date = _ds1302_get_date(ds1302);
    if (month >= 10 && date >= 10) {
        snprintf(buffer, buffer_size, "%d.%d.%d", year, month, date);
    }
    else if (month < 10 && date < 10) {
        snprintf(buffer, buffer_size, "%d.0%d.0%d", year, month, date);
    }
    else if (month >= 10 && date < 10) {
        snprintf(buffer, buffer_size, "%d.%d.0%d", year, month, date);
    }
    else if (month < 10 && date >= 10) {
        snprintf(buffer, buffer_size, "%d.0%d.%d", year, month, date);
    }
};

void ds1302_get_date_and_time(DS1302Instance* ds1302, char* buffer, uint8_t buffer_size) {
    char tmp_buff[64];
    ds1302_get_date(ds1302, buffer, buffer_size);
    ds1302_get_time(ds1302, tmp_buff, 64);
    strcat(buffer, "_");
    strcat(buffer, tmp_buff);
};
