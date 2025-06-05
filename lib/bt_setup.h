#ifndef _PICO_BT_SETUP_H
#define _PICO_BT_SETUP_H

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "btstack_config.h"
#include "pico/stdlib.h"
#include "inttypes.h"
#include <stdio.h>
#include <stdlib.h>

uint8_t bt_check_connection(void);

uint8_t bt_start(void);

uint8_t bt_send_command(const uint8_t* data, const uint8_t dataSize, uint8_t* response);

#endif