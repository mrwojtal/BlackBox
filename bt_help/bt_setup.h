#ifndef _PICO_BT_SETUP_H
#define _PICO_BT_SETUP_H

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "inttypes.h"
#include <stdlib.h>
#include <string.h>

uint8_t bt_start();

void send_command_BT(const uint8_t* data, const uint8_t dataSize, uint8_t* response);

#endif