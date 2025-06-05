#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_cyw43.h"
#include "btstack.h"
#include "bt_setup.h"

#include "elm327.h"

#if 0
#define DEBUG_LOG(...) printf(__VA_ARGS__)
#else
#define DEBUG_LOG(...)
#endif

#define RESPONSE_BUFFER_SIZE 64

int main() {
    char* response_buffer = calloc(RESPONSE_BUFFER_SIZE, sizeof(char));

    stdio_init_all();
    sleep_ms(10000);
    printf("Start\n");

    if (cyw43_arch_init()) {
        puts("cyw43 init error");
        return 0;
    }

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);

    printf("bt_start\n");
    if (0 == bt_start()) {
        printf("Bluetooth OBDII interface connected.\n");
    }

    if (0 == send_AT_command(ATCommandSet.ATZ, response_buffer, RESPONSE_BUFFER_SIZE, send_command_BT)) {
        printf("Success, %s command was sent and received response: %s\n", ATCommandSet.ATZ->name, response_buffer);
    }

    if (0 == send_AT_command(ATCommandSet.ATE0, response_buffer, RESPONSE_BUFFER_SIZE, send_command_BT)) {
        printf("Success, %s command was sent and received response: %s\n", ATCommandSet.ATE0->name, response_buffer);
    }

    if (0 == send_AT_command(ATCommandSet.ATL0, response_buffer, RESPONSE_BUFFER_SIZE, send_command_BT)) {
        printf("Success, %s command was sent and received response: %s\n", ATCommandSet.ATL0->name, response_buffer);
    }

    if (0 == send_AT_command(ATCommandSet.ATI, response_buffer, RESPONSE_BUFFER_SIZE, send_command_BT)) {
        printf("Success, %s command was sent and received response: %s\n", ATCommandSet.ATI->name, response_buffer);
    }

    if (0 == send_AT_command(ATCommandSet.AT1, response_buffer, RESPONSE_BUFFER_SIZE, send_command_BT)) {
        printf("Success, %s command was sent and received response: %s\n", ATCommandSet.AT1->name, response_buffer);
    }

    if (0 == send_AT_command(ATCommandSet.ATRV, response_buffer, RESPONSE_BUFFER_SIZE, send_command_BT)) {
        printf("Success, %s command was sent and received response: %s\n", ATCommandSet.ATRV->name, response_buffer);
    }

    if (0 == send_AT_command(ATCommandSet.ATIGN, response_buffer, RESPONSE_BUFFER_SIZE, send_command_BT)) {
        printf("Success, %s command was sent and received response: %s\n", ATCommandSet.ATIGN->name, response_buffer);
    }

    uint16_t counter = 0;
    while(1) {
        if (0 == send_AT_command(ATCommandSet.ATZ, response_buffer, RESPONSE_BUFFER_SIZE, send_command_BT)) {
            printf("Success, %s command was sent and received response: %s\n", ATCommandSet.ATZ->name, response_buffer);
        }
    
        if (0 == send_AT_command(ATCommandSet.ATE0, response_buffer, RESPONSE_BUFFER_SIZE, send_command_BT)) {
            printf("Success, %s command was sent and received response: %s\n", ATCommandSet.ATE0->name, response_buffer);
        }
    
        if (0 == send_AT_command(ATCommandSet.ATL0, response_buffer, RESPONSE_BUFFER_SIZE, send_command_BT)) {
            printf("Success, %s command was sent and received response: %s\n", ATCommandSet.ATL0->name, response_buffer);
        }
    
        if (0 == send_AT_command(ATCommandSet.ATI, response_buffer, RESPONSE_BUFFER_SIZE, send_command_BT)) {
            printf("Success, %s command was sent and received response: %s\n", ATCommandSet.ATI->name, response_buffer);
        }
    
        if (0 == send_AT_command(ATCommandSet.AT1, response_buffer, RESPONSE_BUFFER_SIZE, send_command_BT)) {
            printf("Success, %s command was sent and received response: %s\n", ATCommandSet.AT1->name, response_buffer);
        }
    
        if (0 == send_AT_command(ATCommandSet.ATRV, response_buffer, RESPONSE_BUFFER_SIZE, send_command_BT)) {
            printf("Success, %s command was sent and received response: %s\n", ATCommandSet.ATRV->name, response_buffer);
        }
    
        if (0 == send_AT_command(ATCommandSet.ATIGN, response_buffer, RESPONSE_BUFFER_SIZE, send_command_BT)) {
            printf("Success, %s command was sent and received response: %s\n", ATCommandSet.ATIGN->name, response_buffer);
        }
        printf("Loop iteration: %d\n", counter);
        counter++;
        busy_wait_ms(1000);
    }
}