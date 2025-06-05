#ifndef LIS3DH_I2C
#define LIS3DH_I2C

#include "pico/stdlib.h"
#include "hardware/i2c.h"

/*======I2C SLAVE ADRESS=====*/
/*! \def LIS3DH_ADDRESS
*   \brief LIS3DH I2C adress.
**/
#define LIS3DH_ADDRESS _u(0x18)

/*======HARDWARE REGISTERS======*/

/*! \def WHO_AM_I
*   \brief LIS3DH identification register, it's value is hardcoded to 0x33. 
*   Register can be read to check communication with LIS3DH.
**/
#define WHO_AM_I _u(0x0F)

/*! \def TEMP_CFG_REG
*   \brief LIS3DH temperature config register used to enable 
*   temperature measurement and ADC enabling.
**/
#define TEMP_CFG_REG _u(0x1F)

/*! \def CTRL_REG_1 
*   \brief LIS3DH control register used to set data rate, power mode
*   and enabling X, Y, Z axis acceleration sensing.
**/
#define CTRL_REG_1 _u(0x20)

/*! \def CTRL_REG_4 
*   \brief LIS3DH control register used to set data update way.
**/
#define CTRL_REG_4 _u(0x23)

/*!
*   \def TEMP_DATA_REG
*   \brief LIS3DH register containing temperature value.
**/
#define TEMP_DATA_REG _u(0x0C)

/*!
*   \def X_ACC_REG
*   \brief LIS3DH register containing X axis acceleration value.
**/
#define X_ACC_REG _u(0x28)

/*!
*   \def Y_ACC_REG
*   \brief LIS3DH register containing Y axis acceleration value.
**/
#define Y_ACC_REG _u(0x2A)

/*!
*   \def Z_ACC_REG
*   \brief LIS3DH register containing Z axis acceleration value.
**/
#define Z_ACC_REG _u(0x2C)

/*!
*   \def G
*   \brief Value of G on planet Earth.
**/
#define G 9.81

/*======FUNCTION SET======*/
void lis3dh_i2c_init(i2c_inst_t *i2c);

void lis3dh_i2c_check(i2c_inst_t *i2c, uint8_t* ret_buff);

void lis3dh_i2c_read(i2c_inst_t *i2c, uint8_t reg, float* ret_value);

#endif