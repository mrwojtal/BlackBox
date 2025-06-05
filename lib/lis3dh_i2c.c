#include "lis3dh_i2c.h"

/// @brief Function checks communication with LIS3DH by reading value of LIS3DH WHO_AM_I register.
/// @param i2c I2C instance: refer to hardware_i2c.h
/// @param ret_buff Pointer to uint8_t variable to store value of LIS3DH dummy register.
void lis3dh_i2c_check(i2c_inst_t *i2c, uint8_t* ret_buff) {
    uint8_t reg = WHO_AM_I;
    i2c_write_blocking(i2c, LIS3DH_ADDRESS, &reg, 1, true);
    i2c_read_blocking(i2c, LIS3DH_ADDRESS, ret_buff, 1, false);
}

/// @brief Function configures LIS3DH control registers to enable temperature, and X, Y, Z acceleration reading in normal mode.
/// @param i2c I2C instance: refer to hardware_i2c.h
void lis3dh_i2c_init(i2c_inst_t *i2c) {
    //buffer used for writing data via i2c api
    uint8_t buffer[2];

    //configure normal mode and 1,25kHz data rate
    buffer[0] = CTRL_REG_1;
    buffer[1] = 0x97; //b10010111 -> 1,25kHz, normal mode, X,Y,Z axis enabled
    i2c_write_blocking(i2c, LIS3DH_ADDRESS, buffer, 2, false); //write confi
    
    //configure blocking data on update for temperature sensor
    buffer[0] = CTRL_REG_4;
    buffer[1] = 0x80; //b10000000 -> block data update until MSB and LSB is read from registers
    i2c_write_blocking(i2c, LIS3DH_ADDRESS, buffer, 2, false);

    //enable adc and configure temperature sensor
    buffer[0] = TEMP_CFG_REG;
    buffer[1] = 0xC0;//b11000000 -> ADC enabled, temperature sensor enabled
    i2c_write_blocking(i2c, LIS3DH_ADDRESS, buffer, 2, false);
}

/// @brief Function used to read acceleration and temperature data from LIS3DH registers.
/// @param i2c I2C instance: refer to hardware_i2c.h
/// @param reg LIS3DH register to read data from.
/// @param ret_value Pointer to float variable in which the LIS3DH register value will be stored.
void lis3dh_i2c_read(i2c_inst_t *i2c, uint8_t reg, float* ret_value) {
    //variables used for data read/write via i2c api
    uint8_t lsb_reg;
    uint8_t msb_reg;
    uint16_t raw_value;

    //read data from the lsb register
    i2c_write_blocking(i2c, LIS3DH_ADDRESS, &reg, 1, true);
    i2c_read_blocking(i2c, LIS3DH_ADDRESS, &lsb_reg, 1, false);
    reg |= 0x01; //logic OR with b00000001 to get data from msb register
    i2c_write_blocking(i2c, LIS3DH_ADDRESS, &reg, 1, true);
    i2c_read_blocking(i2c, LIS3DH_ADDRESS, &msb_reg, 1, false);

    //write msb and lsb data to 16bit variable
    raw_value = (msb_reg<<8) | lsb_reg;
    
    //variables used for converting raw data read from sensors
    float scaling;
    float sensitivity = 0.004f; //sensitivity configured by default to 4mg/digit

    //calculation based on read value
    if (reg == (TEMP_DATA_REG | 0x01)) {
        scaling = 64;
    }
    else if (reg == (X_ACC_REG | 0x01) || reg == (Y_ACC_REG | 0x01) || reg == (Z_ACC_REG | 0x01)) {
        scaling = (64/sensitivity)/G;
    }
    //return acceleration value in m/s^2
    *ret_value = (float) ((int16_t) raw_value) / scaling;
}