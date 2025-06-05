#include "pico/stdlib.h"
#include "ds1302.h"


int main() {

    stdio_init_all();
    sleep_ms(10000);
    printf("Start\n");

    uint8_t date[7] = {25, 6, 5, 4, 14, 50, 0};

    char buffer[64];

    DS1302Instance ds1302 = {13, 14, 15};
    ds1302_init(&ds1302);
    // ds1302_set_time(&ds1302, date);

    while(true) {
        // ds1302_set_time(&ds1302, date);
        ds1302_get_date(&ds1302, buffer, 64);
        printf("Today's date: %s\n", buffer);
        ds1302_get_time(&ds1302, buffer, 64);
        printf("Current time: %s\n", buffer);
        ds1302_get_date_and_time(&ds1302, buffer, 64);
        printf("Today's date and current time: %s\n", buffer);
        printf("\n");
        sleep_ms(5000);
    }

}