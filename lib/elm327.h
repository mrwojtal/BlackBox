#ifndef ELM327
#define ELM327

#include "pico/stdlib.h"
#include <stdio.h>

#define RESPONSE_BUFFER_SIZE 64

/*==================Forward declarations==================*/
struct ATCommand;
struct OBDIICommand;

/*==================Decode callbacks typedef==================*/
typedef uint8_t (*decode_OBDII_command)(const struct OBDIICommand* command, uint8_t* response, char* response_buffer, uint8_t response_buffer_size);

typedef uint8_t (*decode_AT_command)(const struct ATCommand* command, uint8_t* response, char* response_buffer, uint8_t response_buffer_size);

typedef uint8_t (*transmit_callback)(const uint8_t* data, const uint8_t data_size, uint8_t* response);

/// @brief Struct for ELM327 AT command containing command name, data bytes, payload length and decode callback for parsing response.
typedef struct ATCommand {
    const char* name;
    const uint8_t* data;
    const uint8_t data_size;
    const decode_AT_command decode_callback;
} ATCommand;

/// @brief Struct for holding pointers to ATCommand structs.
struct ATCommandSet {
    const ATCommand* ATZ;
    const ATCommand* ATE0;
    const ATCommand* ATE1;
    const ATCommand* ATL0;
    const ATCommand* ATL1;
    const ATCommand* ATI;
    const ATCommand* AT1;
    const ATCommand* ATRV;
    const ATCommand* ATIGN;
    const ATCommand* ATSP0;
    const ATCommand* ATS0;
    const ATCommand* ATS1;
    const ATCommand* ATAL;
    /*might add more commands if needed*/
};

/// @brief Struct for OBDII command containing command name, data bytes, payload length and decode callback for parsing response.
typedef struct OBDIICommand {
    const char* name;
    const uint8_t* data;
    const uint8_t data_size;
    const decode_OBDII_command decode_callback;
} OBDIICommand;

/// @brief Struct for holding pointers to OBDIICommand structs.
struct OBDIICommandSet {
    const OBDIICommand* EngineLoad;
    const OBDIICommand* CoolantTemp;
    const OBDIICommand* IntakePress;
    const OBDIICommand* EngineSpeed;
    const OBDIICommand* VehicleSpeed;
    const OBDIICommand* IntakeAirTemp;
    const OBDIICommand* MassAirFlow;
    const OBDIICommand* DistanceWMIL;
    const OBDIICommand* FuelRailPress;
};

uint8_t send_AT_command(const ATCommand* command, char* response_buffer, uint8_t response_buffer_size, transmit_callback tcb);

uint8_t send_OBDII_command(const OBDIICommand* command, char* response_buffer, uint8_t response_buffer_size, transmit_callback tcb);

/*==========================EXTERN CONST STRUCTS==========================*/
extern const struct OBDIICommandSet OBDIICommandSet;
extern const struct ATCommandSet ATCommandSet;

#endif