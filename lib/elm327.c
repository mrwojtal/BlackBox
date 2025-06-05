#include "elm327.h"

#define DEBUG_ELM

static uint8_t _asc_to_hex(uint8_t value) {
    if (value >= 0x41 && value <= 0x46) {
        return value-0x37;
        printf("asc_to_hex got %c = %d and returned: %d\n", value, value, value-0x37);
    }
    else if (value >= 0x30 && value <= 0x39){
        return value-0x30;
        printf("asc_to_hex got %c = %d and returned: %d\n", value, value, value-0x30);
    }
    else {
        return 0; 
    }
}

static void _packet_to_str(uint8_t* data, char* buffer) {
    uint8_t i = 0;
    uint8_t j = 0;
    #ifdef DEBUG_ELM
    printf("In function _packet_to_str\n");
    #endif
    while(*(data+i) != 0x3E) {
        if (*(data+i) == 0x0D) {
            i++;
            continue;
        }
        *(buffer+j) = *(data+i);
        i++;
        j++;
    }
    *(buffer+j) = '\0';
}

static uint8_t decode_AT_response(const ATCommand* command, uint8_t* response, char* response_buffer, uint8_t response_buffer_size) {
    uint8_t status = 1;
    #ifdef DEBUG_ELM
    printf("In function decode_AT_response\n");
    #endif
    if (response == NULL) {
        #ifdef DEBUG_ELM
        printf("In function decode_AT_response response is NULL!\n");
        #endif
        status = 1;
    }
    else {
        _packet_to_str(response, response_buffer);
        status = (response_buffer != NULL ? 0 : 1);
    }
    return status;
}

static uint8_t decode_OBDII_temperature(const OBDIICommand* command, uint8_t* response, char* response_buffer, uint8_t response_buffer_size) {
    uint8_t status = 1;
    #ifdef DEBUG_ELM
    printf("In function decode_OBDII_temperature\n");
    #endif
    if (response == NULL) {
        #ifdef DEBUG_ELM
        printf("In function decode_OBDII_temperature response is NULL!\n");
        #endif
        status = 1;
    }
    else if (response[0] != 0x34 && response[1] != 0x31) {
        #ifdef DEBUG_ELM
        printf("In function decode_OBDII_temperature response is not positive!\n");
        #endif
        status = 1;
    }
    else {
        snprintf(response_buffer, response_buffer_size, "%d", 
                ((_asc_to_hex(response[4]))*0x10 + (_asc_to_hex(response[5])))-0x28);
        status = 0;
    }
    return status;
}

static uint8_t decode_OBDII_engine_load(const OBDIICommand* command, uint8_t* response, char* response_buffer, uint8_t response_buffer_size) {
    uint8_t status = 1;
    #ifdef DEBUG_ELM
    printf("In function decode_OBDII_engine_load\n");
    #endif
    if (response == NULL) {
        #ifdef DEBUG_ELM
        printf("In function decode_OBDII_engine_load response is NULL!\n");
        #endif
        status = 1;
    }
    else if (response[0] != 0x34 && response[1] != 0x31) {
        #ifdef DEBUG_ELM
        printf("In function decode_OBDII_engine_load response is not positive!\n");
        #endif
        status = 1;
    }
    else {
        snprintf(response_buffer, response_buffer_size, "%.1f", // 100A/255
                (float)((((_asc_to_hex(response[4]))*0x10 + (_asc_to_hex(response[5])))*0x64)/0xFF));
        status = 0;
    }
    return status;
}

static uint8_t no_decode_OBDII_response(const OBDIICommand* command, uint8_t* response, char* response_buffer, uint8_t response_buffer_size) {
    uint8_t status = 1;
    #ifdef DEBUG_ELM
    printf("In function no_decode_OBDII_response\n");
    #endif
    if (response == NULL) {
        #ifdef DEBUG_ELM
        printf("In function no_decode_OBDII_response response is NULL!\n");
        #endif
        status = 1;
    }
    else if (response[0] != 0x34 && response[1] != 0x31) {
        #ifdef DEBUG_ELM
        printf("In function no_decode_OBDII_response response is not positive!\n");
        #endif
        status = 1;
    }
    else {
        snprintf(response_buffer, response_buffer_size, "%d", //A
                ((_asc_to_hex(response[4]))*0x10 + (_asc_to_hex(response[5]))));
        
        status = 0;
    }
    return status;
}

static uint8_t decode_OBDII_engine_speed(const OBDIICommand* command, uint8_t* response, char* response_buffer, uint8_t response_buffer_size) {
    uint8_t status = 1;
    #ifdef DEBUG_ELM
    printf("In function decode_OBDII_engine_speed\n");
    #endif
    if (response == NULL) {
        #ifdef DEBUG_ELM
        printf("In function decode_OBDII_engine_speed response is NULL!\n");
        #endif
        status = 1;
    }
    else if (response[0] != 0x34 && response[1] != 0x31) {
        #ifdef DEBUG_ELM
        printf("In function decode_OBDII_engine_speed response is not positive!\n");
        #endif
        status = 1;
    }
    else {
        snprintf(response_buffer, response_buffer_size, "%d", //(256*A + B)/4
                ((((_asc_to_hex(response[4]))*0x10 + (_asc_to_hex(response[5])))*0x100)+((_asc_to_hex(response[6]))*0x10 + (_asc_to_hex(response[7]))))/0x04);
        status = 0;
    }
    return status;
}

static uint8_t decode_OBDII_distance(const OBDIICommand* command, uint8_t* response, char* response_buffer, uint8_t response_buffer_size) {
    uint8_t status = 1;
    #ifdef DEBUG_ELM
    printf("In function decode_OBDII_distance\n");
    #endif
    if (response == NULL) {
        #ifdef DEBUG_ELM
        printf("In function decode_OBDII_distance response is NULL!\n");
        #endif
        status = 1;
    }
    else if (response[0] != 0x34 && response[1] != 0x31) {
        #ifdef DEBUG_ELM
        printf("In function decode_OBDII_distance response is not positive!\n");
        #endif
        status = 1;
    }
    else {
        snprintf(response_buffer, response_buffer_size, "%d", //256*A + B
                (((_asc_to_hex(response[4]))*0x10 + (_asc_to_hex(response[5])))*0x100)+((_asc_to_hex(response[6]))*0x10 + (_asc_to_hex(response[7]))));
        status = 0;
    }
    return status;
}

static uint8_t decode_OBDII_maf(const OBDIICommand* command, uint8_t* response, char* response_buffer, uint8_t response_buffer_size) {
    uint8_t status = 1;
    #ifdef DEBUG_ELM
    printf("In function decode_OBDII_maf\n");
    #endif
    if (response == NULL) {
        #ifdef DEBUG_ELM
        printf("In function decode_OBDII_maf response is NULL!\n");
        #endif
        status = 1;
    }
    else if (response[0] != 0x34 && response[1] != 0x31) {
        #ifdef DEBUG_ELM
        printf("In function decode_OBDII_maf response is not positive!\n");
        #endif
        status = 1;
    }
    else {
        snprintf(response_buffer, response_buffer_size, "%d", //(256*A + B)/100
                ((((_asc_to_hex(response[4]))*0x10 + (_asc_to_hex(response[5])))*0x100)+((_asc_to_hex(response[6]))*0x10 + (_asc_to_hex(response[7]))))/0x64);
        status = 0;
    }
    return status;
}

static uint8_t decode_OBDII_fuel_rail_press(const OBDIICommand* command, uint8_t* response, char* response_buffer, uint8_t response_buffer_size) {
    uint8_t status = 1;
    #ifdef DEBUG_ELM
    printf("In function decode_OBDII_fuel_rail_press\n");
    #endif
    if (response == NULL) {
        #ifdef DEBUG_ELM
        printf("In function decode_OBDII_fuel_rail_press response is NULL!\n");
        #endif
        status = 1;
    }
    else if (response[0] != 0x34 && response[1] != 0x31) {
        #ifdef DEBUG_ELM
        printf("In function decode_OBDII_fuel_rail_press response is not positive!\n");
        #endif
        status = 1;
    }
    else {
        snprintf(response_buffer, response_buffer_size, "%d", //(256A + B)*10
                ((((_asc_to_hex(response[4]))*0x10 + (_asc_to_hex(response[5])))*0x100)+((_asc_to_hex(response[6]))*0x10 + (_asc_to_hex(response[7]))))/0x0A);
        status = 0;
    }
    return status;
}

/// @brief Function used to send AT commands to ELM327.
/// @param command struct ATCommand parsed to function.
/// @param response_buffer char* buffer to which the ELM327 response will be parsed.
/// @param response_buffer_size size of given buffer in bytes.
/// @param tcb callback for transmit medium, typically function defined to send data over transfer medium, callback shall take data in bytes, data length and response buffer.
///            Response buffer shall be filled with ELM327 repsonse, function shall return 0 if everything went as expected, else return 1 to indicate any fault.
/// @return 0 if command was sent and received positive reponse, otherwise 1.
uint8_t send_AT_command(const ATCommand* command, char* response_buffer, uint8_t response_buffer_size, transmit_callback tcb) {
    uint8_t status = 0;
    uint8_t response[RESPONSE_BUFFER_SIZE];
    if (1 == tcb(command->data, command->data_size, response)) {
        #ifdef DEBUG_ELM
        printf("FAIL: Reponse for command %s not received!\n", command->name);
        #endif
        return 1;
    }
    #ifdef DEBUG_ELM
    printf("In function send_AT_command, after callback\n");
    uint8_t i = 0;
    while(true) {
        printf("[%02X] ", response[i]);
        if (response[i] == 0x3E) {
            break;
        }
        i++;
    }
    #endif
    if (1 == command->decode_callback(command, response, response_buffer, response_buffer_size)) {
        #ifdef DEBUG_ELM
        printf("decode_callback failed\n");
        #endif
        status = 1;
    }
    return status;
}

/// @brief Function used to send OBDII commands to ELM327.
/// @param command struct OBDII command parsed to function.
/// @param response_buffer char* buffer to which the ELM327 response will be parsed.
/// @param response_buffer_size size of given buffer in bytes.
/// @param tcb callback for transmit medium, typically function defined to send data over transfer medium, callback shall take data in bytes, data length and response buffer.
///            Response buffer shall be filled with ELM327 repsonse, function shall return 0 if everything went as expected, else return 1 to indicate any fault.
/// @return 0 if command was sent and received positive reponse, otherwise 1.
uint8_t send_OBDII_command(const OBDIICommand* command, char* response_buffer, uint8_t response_buffer_size, transmit_callback tcb) {
    uint8_t status = 0;
    uint8_t response[RESPONSE_BUFFER_SIZE];
    if (1 == tcb(command->data, command->data_size, response)) {
        #ifdef DEBUG_ELM
        printf("FAIL: Reponse for command %s not received!\n", command->name);
        #endif
        return 1;
    }
    #ifdef DEBUG_ELM
    printf("In function send_OBDII_command, after callback\n");
    uint8_t i = 0;
    while(true) {
        printf("[%02X] ", response[i]);
        if (response[i] == 0x3E) {
            break;
        }
        i++;
    }
    #endif
    if (1 == command->decode_callback(command, response, response_buffer, response_buffer_size)) {
        #ifdef DEBUG_ELM
        printf("decode_callback failed\n");
        #endif
        status = 1;
    }
    return status;
}

const static uint8_t atCommandList[13][6] = {{0x41, 0x54, 0x5A, 0x0D},             //ATZ
                                             {0x41, 0x54, 0x45, 0x30, 0x0D},       //ATE0
                                             {0x41, 0x54, 0x45, 0x31, 0x0D},       //ATE1
                                             {0x41, 0x54, 0x4C, 0x30, 0x0D},       //ATL0
                                             {0x41, 0x54, 0x4C, 0x31, 0x0D},       //ATL1
                                             {0x41, 0x54, 0x49, 0x0D},             //ATI
                                             {0x41, 0x54, 0x40, 0x31, 0x0D},       //AT@1
                                             {0x41, 0x54, 0x52, 0x56, 0x0D},       //ATRV
                                             {0x41, 0x54, 0x49, 0x47, 0x4E, 0x0D}, //ATIGN
                                             {0x41, 0x54, 0x53, 0x50, 0x30, 0x0D}, //ATSP0
                                             {0x41, 0x54, 0x53, 0x30, 0x0D},       //ATS0
                                             {0x41, 0x54, 0x53, 0x31, 0x0D},       //ATS1
                                             {0x41, 0x54, 0x41, 0x4C, 0x0D}        //ATAL
                                           };

const ATCommand ATZ =   {"ATZ - Hard reset",             atCommandList[0],  4, decode_AT_response};
const ATCommand ATE0 =  {"ATE0 - Echo off",              atCommandList[1],  5, decode_AT_response};
const ATCommand ATE1 =  {"ATE1 - Echo on",               atCommandList[2],  5, decode_AT_response};
const ATCommand ATL0 =  {"ATL0 - LineFeed off",          atCommandList[3],  5, decode_AT_response};
const ATCommand ATL1 =  {"ATL1 - LineFeed on",           atCommandList[4],  5, decode_AT_response};
const ATCommand ATI =   {"ATI - Identify device",        atCommandList[5],  4, decode_AT_response};
const ATCommand AT1 =   {"AT@1 - Device description",    atCommandList[6],  5, decode_AT_response};
const ATCommand ATRV =  {"ATRV - Read Input Voltage",    atCommandList[7],  5, decode_AT_response};
const ATCommand ATIGN = {"ATIGN - Read IGN input level", atCommandList[8],  6, decode_AT_response};
const ATCommand ATSP0 = {"ATSP0 - Set auto protocole",   atCommandList[9],  6, decode_AT_response};
const ATCommand ATS0 =  {"ATS0 - Spaces off",            atCommandList[10], 5, decode_AT_response};
const ATCommand ATS1 =  {"ATS1 - Spaces on",             atCommandList[11], 5, decode_AT_response};
const ATCommand ATAL =  {"ATAL - Allow long messages",   atCommandList[12], 5, decode_AT_response};

const struct ATCommandSet ATCommandSet = {
    &ATZ,
    &ATE0,
    &ATE1,
    &ATL0,
    &ATL1,
    &ATI,
    &AT1,
    &ATRV,
    &ATIGN,
    &ATSP0,
    &ATS0,
    &ATS1,
    &ATAL
};

const static uint8_t obdCommandList[9][6] = {{0x30, 0x31, 0x20, 0x30, 0x34, 0x0D}, //Engine load
                                             {0x30, 0x31, 0x20, 0x30, 0x35, 0x0D}, //Engine coolant temperature
                                             {0x30, 0x31, 0x20, 0x30, 0x42, 0x0D}, //Intake manifold absolute pressure
                                             {0x30, 0x31, 0x20, 0x30, 0x43, 0x0D}, //Engine speed
                                             {0x30, 0x31, 0x20, 0x30, 0x44, 0x0D}, //Vehicle speed
                                             {0x30, 0x31, 0x20, 0x30, 0x46, 0x0D}, //Intake air temperature
                                             {0x30, 0x31, 0x20, 0x31, 0x30, 0x0D}, //Mass air flow
                                             {0x30, 0x31, 0x20, 0x32, 0x31, 0x0D}, //Distance with MIL
                                             {0x30, 0x31, 0x20, 0x32, 0x33, 0x0D}  //Fuel rail pressure
                                            };

const OBDIICommand EngineLoad =    {"Service 01, PID 04 - Calculated engine load",     obdCommandList[0], 6, decode_OBDII_engine_load};
const OBDIICommand CoolantTemp =   {"Service 01, PID 05 - Engine coolant temperature", obdCommandList[1], 6, decode_OBDII_temperature};
const OBDIICommand IntakePress =   {"Service 01, PID 0B - Intake absolute pressure",   obdCommandList[2], 6, no_decode_OBDII_response};
const OBDIICommand EngineSpeed =   {"Service 01, PID 0C - Engine Speed",               obdCommandList[3], 6, decode_OBDII_engine_speed};
const OBDIICommand VehicleSpeed =  {"Service 01, PID 0D - Vehicle Speed",              obdCommandList[4], 6, no_decode_OBDII_response};
const OBDIICommand IntakeAirTemp = {"Service 01, PID 0F - Intake air temperature",     obdCommandList[5], 6, decode_OBDII_temperature};
const OBDIICommand MassAirFlow =   {"Service 01, PID 10 - MAF - Mass Air Flow",        obdCommandList[6], 6, decode_OBDII_maf};
const OBDIICommand DistanceWMIL =  {"Service 01, PID 21 - Distance with MIL",          obdCommandList[7], 6, decode_OBDII_distance};
const OBDIICommand FuelRailPress = {"Service 01, PID 23 - Fuel rail pressure",         obdCommandList[8], 6, decode_OBDII_fuel_rail_press};

const struct OBDIICommandSet OBDIICommandSet = {
    &EngineLoad,
    &CoolantTemp,
    &IntakePress,
    &EngineSpeed,
    &VehicleSpeed,
    &IntakeAirTemp,
    &MassAirFlow,
    &DistanceWMIL,
    &FuelRailPress
};