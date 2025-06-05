/**
 * SwSW project - Black Box logger
 * Owners - Krzysztof Wojtal, Szymon Baster
 * 
 */

//std libraries include
#include <stdio.h>
// #include <string.h>
#include <stdlib.h>

//pico standard includes
#include "pico/stdlib.h"
#include "pico/binary_info.h"

//watchdog include
#include "hardware/watchdog.h"

//i2c include, acceletrometer header include
#include "hardware/i2c.h"
#include "lis3dh_i2c.h"

//cyw43, bluetooth includes
#include "pico/cyw43_arch.h"
#include "bt_setup.h"

//elm327 communicaton header include
#include "elm327.h"

//ds1302 rtc include
#include "ds1302.h"

//spi sd card library includes
#include "hw_config.h"
#include "f_util.h"
#include "ff.h"

#define DEBUG_MAIN

#define FILE_NAME_SIZE 64

#define MAIN_LOOP_PERIOD 1000

/*todo:
done //dodac rtc
done //dodac foldery na kazdy dzien logowania, w folderach dodac log_file i data_file
done //dodac debug log
done //wprowadzic jeszcze poprawki w data_file i log_file, upewnic sie czy wszystko dziala poprawnie
wyczyscic includy
done //wyczyscic btstack, dodac komentarze
done //dodac miganie dioda w btstack
done //moze dodac wiecej???//dopisac obd komendy i callbacki do dekodowania
done //obczaic co jest 5 z bi_decl i dodac wiecej binary info
unit testy???
*/

#define LOG_INFO 0
#define LOG_WARNING 1
#define LOG_ERROR 2

void logger(const char* filename, uint8_t log_level, const char* buffer, DS1302Instance ds1302) {
    char time_buffer[TIME_RESPONSE_BUFFER_SIZE];
    FIL file;
    FRESULT fr;

    //get current time for log timestamps
    ds1302_get_time(&ds1302, time_buffer, sizeof time_buffer);

    //opening a log file with write access
    fr = f_open(&file, filename, FA_OPEN_APPEND | FA_WRITE);
    if (fr != FR_OK) {
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }

    switch(log_level) {
        case LOG_INFO: {
            if (f_printf(&file, "[%s][INFO] %s\n", time_buffer, buffer) < 0) {
                printf("f_printf failed\n");
            }
            break;
        }
        case LOG_WARNING: {
            if (f_printf(&file, "[%s][WARNING] %s\n", time_buffer, buffer) < 0) {
                printf("f_printf failed\n");
            }
            break;
        }
        case LOG_ERROR: {
            if (f_printf(&file, "[%s][ERROR] %s\n", time_buffer, buffer) < 0) {
                printf("f_printf failed\n");
            }
            break;
        }
        default:
            break;
    }

    //close the log file
    fr = f_close(&file);
    if (fr != FR_OK) {
        panic("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }
}

void save_data(const char* filename, const char* buffer, DS1302Instance ds1302) {
    char time_buffer[TIME_RESPONSE_BUFFER_SIZE];
    FIL file;
    FRESULT fr;

    //get current time for data timestamps
    ds1302_get_time(&ds1302, time_buffer, sizeof time_buffer);

    //opening a data file with write access
    fr = f_open(&file, filename, FA_OPEN_APPEND | FA_WRITE);
    if (fr != FR_OK) {
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    }

    //write data to the file
    if (f_printf(&file, "%s, %s\n", time_buffer, buffer) < 0) {
        printf("f_printf failed\n");
    }

    //close the data file
    fr = f_close(&file);
    if (fr != FR_OK) {
        panic("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }
}

int main(void) {
    /*=========================VARIABLES=========================*/
    //structure for ds1302 handling 
    DS1302Instance ds1302 = {13, 14, 15}; //GPIO13, GPIO14, GPIO15
    //buffer for storing time data
    char time_buffer[TIME_RESPONSE_BUFFER_SIZE];

    //file system related variables
    FATFS fs;
    FRESULT fr;
    FIL file;
    FILINFO fn;
    uint8_t log_nf_flag = 0;
    uint8_t data_nf_flag = 0;
    char directory_name[FILE_NAME_SIZE]; 
    char data_file_name[FILE_NAME_SIZE];
    char log_file_name[FILE_NAME_SIZE];
    char rw_buffer[256];
    //csv file table header
    const char* const table_header = "Timestamp [HH;MM;SS], Xacc [m/s^2], Yacc [m/s^2], Zacc [m/s^2], Tvar [C], Engine load [%%], Coolant temp [C], Intake pressure [kPa], Engine speed [RPM], Vehicle speed [km/h], Intake air temp [C], Distance with MIL [km], MAF [g/s], Fuel pressure [kPa]\n";    

    //i2c lis3dh related variables
    uint8_t lis3dh_check;
    //variables for storing sensor data
    float x_acc, y_acc, z_acc, temperature;

    //buffer for storing OBDII data
    char response_buffer[RESPONSE_BUFFER_SIZE];

    //buffers for stroing specific responses
    char engine_load_buff[8];
    char coolant_temp_buff[8];
    char intake_press_buff[8];
    char engine_speed_buff[8];
    char vehicle_speed_buff[8];
    char intake_air_temp_buff[8];
    char distance_w_mil_buff[8];
    char maf_buff[8];
    char fuel_press_buff[8];

    //main loop iterator
    uint64_t iterator = 0;

    /*=========================PICOTOOL BINARY INFO=========================*/
    //program info
    bi_decl(bi_program_name("Black Box"));
    bi_decl(bi_program_version_string("v1.0"));
    bi_decl(bi_program_description("Car data logger for use with OBD-II diagnostic interface."));
    bi_decl(bi_program_url("https://github.com/mrwojtal/BlackBox.git"));

    //make SPI pins available to picotool
    bi_decl(bi_3pins_with_func(PICO_DEFAULT_SPI_SCK_PIN, 
                               PICO_DEFAULT_SPI_TX_PIN, 
                               PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI));
    bi_decl(bi_1pin_with_name(PICO_DEFAULT_SPI_CSN_PIN, "Slave select"));

    //make I2C pins available to picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

    //make onboard LED pin available to picotool
    bi_decl(bi_1pin_with_name(CYW43_WL_GPIO_LED_PIN, "LED: Fast blink - Bluetooth device connected, Slow blink - Bluetooth device disconnected"));

    //make ds1302 pins available to picotool
    bi_decl(bi_1pin_with_name(13, "1MHz SW driven CLK output for DS1302"));
    bi_decl(bi_1pin_with_name(14, "Data I/O for DS1302"));
    bi_decl(bi_1pin_with_name(15, "RST output for DS1302, when set to High, transmission is possible"));

    /*=========================INITIALIZE DS1302 RTC=========================*/
    //initialize ds1302
    ds1302_init(&ds1302);
    
    //get current date from ds1302
    ds1302_get_date(&ds1302, time_buffer, sizeof time_buffer);
    //set directory name corresponding to current date
    snprintf(directory_name, sizeof directory_name, "%s", time_buffer);

    //get current time from ds1302
    ds1302_get_time_hm(&ds1302, time_buffer, sizeof time_buffer);
    //set data file name corresponding to current date
    snprintf(data_file_name, sizeof data_file_name, "%s/data_%s.csv", directory_name, time_buffer);
    //set log file name corresponding to current date
    snprintf(log_file_name, sizeof log_file_name, "%s/log_%s.log", directory_name, time_buffer);

    /*=========================INITIALIZE FILE SYSTEM ON SD CARD=========================*/
    //register file system and initialize sd card
    fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    //create directory
    fr = f_mkdir(directory_name);
    if (fr == FR_OK) {
        printf("Directory %s created succesfully\n", directory_name);
    }
    else if (fr == FR_EXIST) {
        printf("Directory %s already exists\n", directory_name);
    }
    else {
        printf("f_mkdir(%s) error: %s (%d)\n", directory_name, FRESULT_str(fr), fr);
    }

    // check if data file exists
    fr = f_stat(data_file_name, &fn);
    switch (fr) {
        case FR_OK:
            data_nf_flag = 0;
            printf("Data file %s exists\n", data_file_name);
            // printf("Size: %lu\n", fn.fsize);
            // printf("Timestamp: %u-%02u-%02u, %02u:%02u\n",
            //       (fn.fdate >> 9) + 1980, fn.fdate >> 5 & 15, fn.fdate & 31,
            //        fn.ftime >> 11, fn.ftime >> 5 & 63);
            // printf("Attributes: %c%c%c%c%c\n",
            //       (fn.fattrib & AM_DIR) ? 'D' : '-',
            //       (fn.fattrib & AM_RDO) ? 'R' : '-',
            //       (fn.fattrib & AM_HID) ? 'H' : '-',
            //       (fn.fattrib & AM_SYS) ? 'S' : '-',
            //       (fn.fattrib & AM_ARC) ? 'A' : '-');
            break;
        case FR_NO_FILE:
        case FR_NO_PATH:
            printf("File %s does not exist\n", data_file_name);
            printf("Creating file %s\n", data_file_name);
            data_nf_flag = 1;
            fr = f_open(&file, data_file_name, FA_CREATE_NEW);
            if (fr != FR_OK) {
                panic("f_open(%s) error: %s (%d)\n", data_file_name, FRESULT_str(fr), fr);
            }
            fr = f_close(&file);
            if (fr != FR_OK) {
                panic("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
            }
            break;
        default:
            printf("An error occured %s (%d)\n", FRESULT_str(fr), fr);
    }

    // check if log file exists
    fr = f_stat(log_file_name, &fn);
    switch (fr) {
        case FR_OK:
            log_nf_flag = 0;
            printf("Log file %s exists\n", log_file_name);
            // printf("Size: %lu\n", fn.fsize);
            // printf("Timestamp: %u-%02u-%02u, %02u:%02u\n",
            //       (fn.fdate >> 9) + 1980, fn.fdate >> 5 & 15, fn.fdate & 31,
            //        fn.ftime >> 11, fn.ftime >> 5 & 63);
            // printf("Attributes: %c%c%c%c%c\n",
            //       (fn.fattrib & AM_DIR) ? 'D' : '-',
            //       (fn.fattrib & AM_RDO) ? 'R' : '-',
            //       (fn.fattrib & AM_HID) ? 'H' : '-',
            //       (fn.fattrib & AM_SYS) ? 'S' : '-',
            //       (fn.fattrib & AM_ARC) ? 'A' : '-');
            break;
        case FR_NO_FILE:
        case FR_NO_PATH:
            printf("File %s does not exist\n", log_file_name);
            printf("Creating file %s\n", log_file_name);
            log_nf_flag = 1;
            fr = f_open(&file, log_file_name, FA_CREATE_NEW);
            if (fr != FR_OK) {
                panic("f_open(%s) error: %s (%d)\n", log_file_name, FRESULT_str(fr), fr);
            }
            fr = f_close(&file);
            if (fr != FR_OK) {
                panic("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
            }
            break;
        default:
            printf("An error occured %s (%d)\n", FRESULT_str(fr), fr);
    }

    //opening a data file with read/write access
    fr = f_open(&file, data_file_name, FA_OPEN_APPEND | FA_WRITE | FA_READ);
    if (fr != FR_OK) {
        panic("f_open(%s) error: %s (%d)\n", data_file_name, FRESULT_str(fr), fr);
    }

    //set read/write pointer to start of the file
    fr = f_rewind(&file);
    if (fr != FR_OK) {
        printf("f_rewind(%s) error: %s (%d)\n", data_file_name, FRESULT_str(fr), fr);
    }

    //read first line of data file (table header)
    if (f_gets(rw_buffer, sizeof rw_buffer, &file) == NULL) {
        if (data_nf_flag == 1) {
            printf("New %s file created, adding table header.\n", data_file_name);
            fr = f_rewind(&file);
            if (fr != FR_OK) {
                printf("f_rewind(%s) error: %s (%d)\n", data_file_name, FRESULT_str(fr), fr);
            }
            if (f_printf(&file, table_header) < 0) {
                printf("f_printf failed\n");
            }
        }
        else if (data_nf_flag == 0) {
            printf("f_gets failed\n");
        }
    }
    else {
        if (strcmp(table_header, rw_buffer) != 0) {
            printf("File %s did not contain table header\n", data_file_name);
            printf("Buffer: %s\n", rw_buffer);
            //set read/write pointer to start of the file
            fr = f_rewind(&file);
            if (fr != FR_OK) {
                printf("f_rewind(%s) error: %s (%d)\n", data_file_name, FRESULT_str(fr), fr);
            }
            //write table header to start of the file
            if (f_printf(&file, table_header) < 0) {
                printf("f_printf failed\n");
            }
        }
        else {
            printf("File %s already contained table header\n", data_file_name);
        }
    }

    //DEBUG read the whole file
    // fr = f_rewind(&file);
    // if (fr != FR_OK) {
    //     printf("f_rewind(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    // }
    // while(f_gets(rw_buffer, sizeof rw_buffer, &file)) {
    //     printf("%s", rw_buffer);
    // }

    //close the file
    fr = f_close(&file);
    if (fr != FR_OK) {
        panic("f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    //unmount sd card
    // f_unmount("");

    //begin logging with start checkpoint
    snprintf(rw_buffer, sizeof rw_buffer, "Start of main function");
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);

    /*=========================INITIALIZE HARWARE INTERFACES=========================*/
    //initialize stdio - uart, usb for debug purposes
    snprintf(rw_buffer, sizeof rw_buffer, "Initializing stdio...");
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);

    if (stdio_init_all()) {
        snprintf(rw_buffer, sizeof rw_buffer, "stdio initialized succesfully");
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    else {
        snprintf(rw_buffer, sizeof rw_buffer, "stdio was not initialized succesfully!");
        logger(log_file_name, LOG_ERROR, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }

    sleep_ms(10000);

    #ifdef DEBUG_MAIN
    printf("Current time is: %s", time_buffer);
    #endif

    //initialize cyw43 architecture for Bluetooth support
    snprintf(rw_buffer, sizeof rw_buffer, "Initializing cyw43 architecture...");
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
    #ifdef DEBUG_MAIN
    printf("%s\n", rw_buffer);
    #endif

    if (0 == cyw43_arch_init()) {
        snprintf(rw_buffer, sizeof rw_buffer, "cyw43 architecture initialized succesfully");
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    else {
        snprintf(rw_buffer, sizeof rw_buffer, "cyw43 architecture not initialized succesfully!");
        logger(log_file_name, LOG_ERROR, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
        return 1;
    }
    
    //set onboard LED OFF
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);

    //using SPI0 on the default SCK, MOSI, MISO, CS pins (18, 19, 16, 17 on a pico)
    //spi is initialized with my_spi_init() function call (my_spi.h)

    //using I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
    snprintf(rw_buffer, sizeof rw_buffer, "Initializing I2C instance...");
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
    #ifdef DEBUG_MAIN
    printf("%s\n", rw_buffer);
    #endif

    if (400*1000 == i2c_init(i2c_default, 400 * 1000)) {
        #ifdef DEBUG_MAIN
        printf("I2C instance initialized succesfully\n");
        #endif
        snprintf(rw_buffer, sizeof rw_buffer, "I2C instance initialized succesfully");
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
    }
    else {
        #ifdef DEBUG_MAIN
        printf("I2C instance not initialized succesfully!\n");
        #endif
        snprintf(rw_buffer, sizeof rw_buffer, "I2C instance not initialized succesfully!");
        logger(log_file_name, LOG_ERROR, rw_buffer, ds1302);
    }
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    if(GPIO_FUNC_I2C ==  gpio_get_function(PICO_DEFAULT_I2C_SDA_PIN)) {
        snprintf(rw_buffer, sizeof rw_buffer, "PICO_DEFAULT_I2C_SDA_PIN [%d] initialized succesfully to GPIO_FUNC_I2C [%d]", PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    else {
        snprintf(rw_buffer, sizeof rw_buffer, "PICO_DEFAULT_I2C_SDA_PIN [%d] not initialized succesfully to GPIO_FUNC_I2C [%d]!", PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
        logger(log_file_name, LOG_ERROR, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    if(GPIO_FUNC_I2C ==  gpio_get_function(PICO_DEFAULT_I2C_SDA_PIN)) {
        snprintf(rw_buffer, sizeof rw_buffer, "PICO_DEFAULT_I2C_SCL_PIN [%d] initialized succesfully to GPIO_FUNC_I2C [%d]", PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    else {
        snprintf(rw_buffer, sizeof rw_buffer, "PICO_DEFAULT_I2C_SCL_PIN [%d] not initialized succesfully to GPIO_FUNC_I2C [%d]!", PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
        logger(log_file_name, LOG_ERROR, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);

    //check communication with lis3dh on i2c bus
    snprintf(rw_buffer, sizeof rw_buffer, "Checking communcation with LIS3DH on I2C bus by reading value in it's WHO_AM_I dummy register...");
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
    #ifdef DEBUG_MAIN
    printf("%s\n", rw_buffer);
    #endif

    lis3dh_i2c_check(i2c_default, &lis3dh_check);
    if (0x33 == lis3dh_check) {
        snprintf(rw_buffer, sizeof rw_buffer, "LIS3DH communication working as expected: WHO_AM_I dummy register contains: 0x%02X", lis3dh_check);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    else {
        snprintf(rw_buffer, sizeof rw_buffer, "LIS3DH communication test failed: WHO_AM_I dummy register contains: 0x%02X", lis3dh_check);
        logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }

    //configure lis3dh for reading X,Y,Z acceleration and temperature
    lis3dh_i2c_init(i2c_default);
 
    /*=========================CONNECT TO BLUETOOTH OBDII INTERFACE=========================*/
    //connect to OBDII diagnostic interface
    snprintf(rw_buffer, sizeof rw_buffer, "Setting up Bluetooth stack and connecting to OBDII interface...");
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
    #ifdef DEBUG_MAIN
    printf("%s\n", rw_buffer);
    #endif

    if (0 == bt_start()) {
        snprintf(rw_buffer, sizeof rw_buffer, "Bluetooth OBDII interface connected succesfully");
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    else {
        snprintf(rw_buffer, sizeof rw_buffer, "Bluetooth OBDII interface not connected succesfully");
        logger(log_file_name, LOG_ERROR, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }

    /*=========================SETUP ELM327 WITH AT COMMANDS=========================*/
    snprintf(rw_buffer, sizeof rw_buffer, "Setting up ELM327 with AT commands...");
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
    #ifdef DEBUG_MAIN
    printf("%s\n", rw_buffer);
    #endif

    snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", ATCommandSet.ATZ->name);
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
    #ifdef DEBUG_MAIN
    printf("%s\n", rw_buffer);
    #endif
    if (0 == send_AT_command(ATCommandSet.ATZ, response_buffer, sizeof response_buffer, bt_send_command)) {
        snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s", ATCommandSet.ATZ->name, response_buffer);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    else {
        snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", ATCommandSet.ATZ->name);
        logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }

    snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", ATCommandSet.ATE0->name);
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
    #ifdef DEBUG_MAIN
    printf("%s\n", rw_buffer);
    #endif
    if (0 == send_AT_command(ATCommandSet.ATE0, response_buffer, sizeof response_buffer, bt_send_command)) {
        snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s", ATCommandSet.ATE0->name, response_buffer);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    else {
        snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", ATCommandSet.ATE0->name);
        logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }

    snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", ATCommandSet.ATL0->name);
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
    #ifdef DEBUG_MAIN
    printf("%s\n", rw_buffer);
    #endif
    if (0 == send_AT_command(ATCommandSet.ATL0, response_buffer, sizeof response_buffer, bt_send_command)) {
        snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s", ATCommandSet.ATL0->name, response_buffer);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    else {
        snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", ATCommandSet.ATL0->name);
        logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }

    snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", ATCommandSet.ATI->name);
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
    #ifdef DEBUG_MAIN
    printf("%s\n", rw_buffer);
    #endif
    if (0 == send_AT_command(ATCommandSet.ATI, response_buffer, sizeof response_buffer, bt_send_command)) {
        snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s", ATCommandSet.ATI->name, response_buffer);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    else {
        snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", ATCommandSet.ATI->name);
        logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }

    snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", ATCommandSet.ATRV->name);
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
    #ifdef DEBUG_MAIN
    printf("%s\n", rw_buffer);
    #endif
    if (0 == send_AT_command(ATCommandSet.ATRV, response_buffer, sizeof response_buffer, bt_send_command)) {
        snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s", ATCommandSet.ATRV->name, response_buffer);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    else {
        snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", ATCommandSet.ATRV->name);
        logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }

    
    snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", ATCommandSet.ATIGN->name);
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
    #ifdef DEBUG_MAIN
    printf("%s\n", rw_buffer);
    #endif
    if (0 == send_AT_command(ATCommandSet.ATIGN, response_buffer, sizeof response_buffer, bt_send_command)) {
        snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s", ATCommandSet.ATIGN->name, response_buffer);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    else {
        snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", ATCommandSet.ATIGN->name);
        logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }

    snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", ATCommandSet.ATS0->name);
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
    #ifdef DEBUG_MAIN
    printf("%s\n", rw_buffer);
    #endif
    if (0 == send_AT_command(ATCommandSet.ATS0, response_buffer, sizeof response_buffer, bt_send_command)) {
        snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s", ATCommandSet.ATS0->name, response_buffer);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    else {
        snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", ATCommandSet.ATS0->name);
        logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }

    snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", ATCommandSet.ATAL->name);
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
    #ifdef DEBUG_MAIN
    printf("%s\n", rw_buffer);
    #endif
    if (0 == send_AT_command(ATCommandSet.ATAL, response_buffer, sizeof response_buffer, bt_send_command)) {
        snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s", ATCommandSet.ATAL->name, response_buffer);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    else {
        snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", ATCommandSet.ATAL->name);
        logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }

    snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", ATCommandSet.ATSP0->name);
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
    #ifdef DEBUG_MAIN
    printf("%s\n", rw_buffer);
    #endif
    if (0 == send_AT_command(ATCommandSet.ATSP0, response_buffer, sizeof response_buffer, bt_send_command)) {
        snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s", ATCommandSet.ATSP0->name, response_buffer);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }
    else {
        snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", ATCommandSet.ATSP0->name);
        logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
    }

    /*=========================MAIN LOOP=========================*/
    snprintf(rw_buffer, sizeof rw_buffer, "Step into main loop");
    logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
    #ifdef DEBUG_MAIN
    printf("%s\n", rw_buffer);
    #endif
    
    while (true) {
        //read data from lis3dh sensor registers via i2c bus every 5 seconds
        lis3dh_i2c_read(i2c_default, X_ACC_REG, &x_acc);
        lis3dh_i2c_read(i2c_default, Y_ACC_REG, &y_acc);
        lis3dh_i2c_read(i2c_default, Z_ACC_REG, &z_acc);
        lis3dh_i2c_read(i2c_default, TEMP_DATA_REG, &temperature);

        snprintf(rw_buffer, sizeof rw_buffer, "Temperature variation reading from LIS3DH sensor: %.3f [%cC]", temperature, 176);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
        snprintf(rw_buffer, sizeof rw_buffer, "X axis acceleration reading from LIS3DH sensor: %.3f [m/s^2]", x_acc);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
        snprintf(rw_buffer, sizeof rw_buffer, "Y axis acceleration reading from LIS3DH sensor: %.3f [m/s^2]", y_acc);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
        snprintf(rw_buffer, sizeof rw_buffer, "Z axis acceleration reading from LIS3DH sensor: %.3f [m/s^2]", z_acc);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif

        //read data from OBDII diagnostic interface and print data on stdout
        snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", OBDIICommandSet.VehicleSpeed->name);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
        if (0 == send_OBDII_command(OBDIICommandSet.VehicleSpeed, response_buffer, sizeof response_buffer, bt_send_command)) {
            snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s [km/h]", OBDIICommandSet.VehicleSpeed->name, response_buffer);
            logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
            snprintf(vehicle_speed_buff, sizeof vehicle_speed_buff, "%s", response_buffer);
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }
        else {
            snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", OBDIICommandSet.VehicleSpeed->name);
            logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
            snprintf(vehicle_speed_buff, sizeof vehicle_speed_buff, "NA");
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }

        snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", OBDIICommandSet.EngineSpeed->name);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
        if (0 == send_OBDII_command(OBDIICommandSet.EngineSpeed, response_buffer, sizeof response_buffer, bt_send_command)) {
            snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s [RPM]", OBDIICommandSet.EngineSpeed->name, response_buffer);
            logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
            snprintf(engine_speed_buff, sizeof engine_speed_buff, "%s", response_buffer);
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }
        else {
            snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", OBDIICommandSet.EngineSpeed->name);
            logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
            snprintf(engine_speed_buff, sizeof engine_speed_buff, "NA");
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }

        snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", OBDIICommandSet.EngineLoad->name);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
        if (0 == send_OBDII_command(OBDIICommandSet.EngineLoad, response_buffer, sizeof response_buffer, bt_send_command)) {
            snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s [%%]", OBDIICommandSet.EngineLoad->name, response_buffer);
            logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
            snprintf(engine_load_buff, sizeof engine_load_buff, "%s", response_buffer);
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }
        else {
            snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", OBDIICommandSet.EngineLoad->name);
            logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
            snprintf(engine_load_buff, sizeof engine_load_buff, "NA");
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }

        snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", OBDIICommandSet.CoolantTemp->name);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
        if (0 == send_OBDII_command(OBDIICommandSet.CoolantTemp, response_buffer, sizeof response_buffer, bt_send_command)) {
            snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s [%cC]", OBDIICommandSet.CoolantTemp->name, response_buffer, 176);
            logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
            snprintf(coolant_temp_buff, sizeof coolant_temp_buff, "%s", response_buffer);
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }
        else {
            snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", OBDIICommandSet.CoolantTemp->name);
            logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
            snprintf(coolant_temp_buff, sizeof coolant_temp_buff, "NA");
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }

        snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", OBDIICommandSet.IntakeAirTemp->name);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
        if (0 == send_OBDII_command(OBDIICommandSet.IntakeAirTemp, response_buffer, sizeof response_buffer, bt_send_command)) {
            snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s [%cC]", OBDIICommandSet.IntakeAirTemp->name, response_buffer, 176);
            logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
            snprintf(intake_air_temp_buff, sizeof intake_air_temp_buff, "%s", response_buffer);
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }
        else {
            snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", OBDIICommandSet.IntakeAirTemp->name);
            logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
            snprintf(intake_air_temp_buff, sizeof intake_air_temp_buff, "NA");
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }

        snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", OBDIICommandSet.IntakePress->name);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
        if (0 == send_OBDII_command(OBDIICommandSet.IntakePress, response_buffer, sizeof response_buffer, bt_send_command)) {
            snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s [kPa]", OBDIICommandSet.IntakePress->name, response_buffer);
            logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
            snprintf(intake_press_buff, sizeof intake_press_buff, "%s", response_buffer);
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }
        else {
            snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", OBDIICommandSet.IntakePress->name);
            logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
            snprintf(intake_press_buff, sizeof intake_press_buff, "NA");
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }

        snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", OBDIICommandSet.DistanceWMIL->name);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
        if (0 == send_OBDII_command(OBDIICommandSet.DistanceWMIL, response_buffer, sizeof response_buffer, bt_send_command)) {
            snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s [km]", OBDIICommandSet.DistanceWMIL->name, response_buffer);
            logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
            snprintf(distance_w_mil_buff, sizeof distance_w_mil_buff, "%s", response_buffer);
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }
        else {
            snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", OBDIICommandSet.DistanceWMIL->name);
            logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
            snprintf(distance_w_mil_buff, sizeof distance_w_mil_buff, "NA");
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }

        snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", OBDIICommandSet.MassAirFlow->name);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
        if (0 == send_OBDII_command(OBDIICommandSet.MassAirFlow, response_buffer, sizeof response_buffer, bt_send_command)) {
            snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s [g/s]", OBDIICommandSet.MassAirFlow->name, response_buffer);
            logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
            snprintf(maf_buff, sizeof maf_buff, "%s", response_buffer);
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }
        else {
            snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", OBDIICommandSet.MassAirFlow->name);
            logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
            snprintf(maf_buff, sizeof maf_buff, "NA");
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }

        snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", OBDIICommandSet.FuelRailPress->name);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
        if (0 == send_OBDII_command(OBDIICommandSet.FuelRailPress, response_buffer, sizeof response_buffer, bt_send_command)) {
            snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s [kPa]", OBDIICommandSet.FuelRailPress->name, response_buffer);
            logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
            snprintf(fuel_press_buff, sizeof fuel_press_buff, "%s", response_buffer);
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }
        else {
            snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", OBDIICommandSet.FuelRailPress->name);
            logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
            snprintf(fuel_press_buff, sizeof fuel_press_buff, "NA");
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }

        // snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", ATCommandSet.ATI->name);
        // logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        // #ifdef DEBUG_MAIN
        // printf("%s\n", rw_buffer);
        // #endif
        // if (0 == send_AT_command(ATCommandSet.ATI, response_buffer, sizeof response_buffer, bt_send_command)) {
        //     snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s", ATCommandSet.ATI->name, response_buffer);
        //     logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        //     #ifdef DEBUG_MAIN
        //     printf("%s\n", rw_buffer);
        //     #endif
        // }
        // else {
        //     snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", ATCommandSet.ATI->name);
        //     logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
        //     #ifdef DEBUG_MAIN
        //     printf("%s\n", rw_buffer);
        //     #endif
        // }

        snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", ATCommandSet.ATRV->name);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
        if (0 == send_AT_command(ATCommandSet.ATRV, response_buffer, sizeof response_buffer, bt_send_command)) {
            snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s", ATCommandSet.ATRV->name, response_buffer);
            logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }
        else {
            snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", ATCommandSet.ATRV->name);
            logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }
    
        snprintf(rw_buffer, sizeof rw_buffer, "Sending %s command to ELM327", ATCommandSet.ATIGN->name);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
        #ifdef DEBUG_MAIN
        printf("%s\n", rw_buffer);
        #endif
        if (0 == send_AT_command(ATCommandSet.ATIGN, response_buffer, sizeof response_buffer, bt_send_command)) {
            snprintf(rw_buffer, sizeof rw_buffer, "Success, %s command was sent and received response: %s", ATCommandSet.ATIGN->name, response_buffer);
            logger(log_file_name, LOG_INFO, rw_buffer, ds1302);
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }
        else {
            snprintf(rw_buffer, sizeof rw_buffer, "Warning, %s command was sent but not received response!", ATCommandSet.ATIGN->name);
            logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
        }

        //save data to data file
        snprintf(rw_buffer, sizeof rw_buffer, "%.3f, %.3f, %.3f, %.3f, %s, %s, %s, %s, %s, %s, %s, %s, %s", x_acc, y_acc, z_acc, temperature, engine_load_buff, coolant_temp_buff, intake_press_buff, engine_speed_buff, vehicle_speed_buff, intake_air_temp_buff, distance_w_mil_buff, maf_buff, fuel_press_buff);
        save_data(data_file_name, rw_buffer, ds1302);

        snprintf(rw_buffer, sizeof rw_buffer, "MAIN LOOP ITERATION: %lld", iterator);
        logger(log_file_name, LOG_INFO, rw_buffer, ds1302);

        #ifdef DEBUG_MAIN
        printf("MAIN LOOP ITERATION: %lld\n", iterator);
        printf("\n");
        #endif

        iterator++;

        sleep_ms(MAIN_LOOP_PERIOD);

        if (1 == bt_check_connection()) {
            snprintf(rw_buffer, sizeof rw_buffer, "OBDII interface disconnected, performing chip reset with watchdog in 100ms");
            logger(log_file_name, LOG_WARNING, rw_buffer, ds1302);
            #ifdef DEBUG_MAIN
            printf("%s\n", rw_buffer);
            #endif
            watchdog_enable(100, false);
            while(true); //stuck in infinte loop, waiting for rebooting by watchdog
        }
    }
}
