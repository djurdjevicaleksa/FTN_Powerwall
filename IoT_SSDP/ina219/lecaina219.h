#ifndef _LECAINA_219_H
#define _LECAINA_219_H

/*
  ------------------------------------------------------------------------------------------------------
  -> File:            leca_ina219.h
 
  -> Author:          Aleksa Djurdjevic
  
  -> Last change:     16.4.2024.
  
  -> Description:     
    Header file of a library for controlling and using
    the Texas Instruments INA219 current and voltage sensor.

  -> Additional info: 
    Made as part of a project at Faculty of Technical Sciences in Novi Sad, Serbia.

  -> INA219 data sheet: https://www.ti.com/lit/ds/symlink/ina219.pdf
    -----------------------------------------------------------------------------------------------------
*/

#include <stdint.h>

//-------REGISTERS-------
#define REGISTER_CONFIGURATION          0x00
#define REGISTER_SHUNT_VOLTAGE          0x01
#define REGISTER_BUS_VOLTAGE            0x02
#define REGISTER_POWER                  0x03
#define REGISTER_CURRENT                0x04
#define REGISTER_CALIBRATION            0x05
//-----------------------

//-----CONFIGURATION-----

#define CONFIG_NO_RESET                 0
#define CONFIG_RESET                    1

#define CONFIG_VOLTAGE_16V              0
#define CONFIG_VOLTAGE_32V              1

#define CONFIG_GAIN_1                   0
#define CONFIG_GAIN_2                   1
#define CONFIG_GAIN_4                   2
#define CONFIG_GAIN_8                   3

#define CONFIG_ADC_9BIT                 0
#define CONFIG_ADC_10BIT                1
#define CONFIG_ADC_11BIT                2
#define CONFIG_ADC_12BIT                3
#define CONFIG_ADC_12BIT_2SAMPLES       9
#define CONFIG_ADC_12BIT_4SAMPLES       10
#define CONFIG_ADC_12BIT_8SAMPLES       11
#define CONFIG_ADC_12BIT_16SAMPLES      12
#define CONFIG_ADC_12BIT_32SAMPLES      13
#define CONFIG_ADC_12BIT_64SAMPLES      14
#define CONFIG_ADC_12BIT_128SAMPLES     15

#define CONFIG_MODE_PWR_DOWN            0
#define CONFIG_MODE_SHUNT_TRIG          1
#define CONFIG_MODE_BUS_TRIG            2
#define CONFIG_MODE_SHUNT_BUS_TRIG      3
#define CONFIG_MODE_ADC_OFF             4
#define CONFIG_MODE_SHUNT_CONT          5
#define CONFIG_MODE_BUS_CONT            6
#define CONFIG_MODE_SHUNT_BUS_CONT      7

//-----------------------

//-------ADDRESSES-------
#define I2C_ADDR0  0b1000000 //0x40
#define I2C_ADDR1  0b1000001
#define I2C_ADDR2  0b1000010
#define I2C_ADDR3  0b1000011
#define I2C_ADDR4  0b1000100
#define I2C_ADDR5  0b1000101
#define I2C_ADDR6  0b1000110
#define I2C_ADDR7  0b1000111
#define I2C_ADDR8  0b1001000
#define I2C_ADDR9  0b1001001
#define I2C_ADDR10 0b1001010
#define I2C_ADDR11 0b1001011
#define I2C_ADDR12 0b1001100
#define I2C_ADDR13 0b1001101
#define I2C_ADDR14 0b1001110
#define I2C_ADDR15 0b1001111

//-----------------------

#define CALIBRATION_CONSTANT 0.04096
#define CALIBRATION_LSB 0xFFFE
#define CALIBRATION_LSB_DIVIDER 32770

#define INA_FAILURE 0
#define INA_SUCCESS 1

#define MILLI_MULTIPLIER 1000.0
#define SHUNT_MULTIPLIER 500.0
#define SHUNT_VOLTAGE_LSB 0.00001

float BUS_VOLTAGES[2] = {16, 32};
float GAIN[4] = {0.04, 0.08, 0.16, 0.32};

typedef struct ina LecaINA219;

// Used to retrieve multiple readings from the sensor using inaGetData().
typedef struct
{
    float busVoltage_V;
    float busVoltage_mV;
    float current_A;
    float current_mA;
    float power_W;
    float power_mW;
    float shuntVoltage_mV;

}DATAPACK;

/*
    Allocates memory for an instance of LecaINA219 struct with important parameters.

    @param shuntResistance Resistance of the shunt resistor in ohms.
    @param maxExpectedAmps Maximum current the sensor will measure. Lower current means the reading will be more precise.

    @return Returns address of the dynamically allocated instance.
*/
LecaINA219* createINA(float shuntResistance, float maxExpectedAmps);

/*
    Sets up I2C communication with the target device.

    @param ina Instance of LecaINA219 struct.
    @param i2cAddress I2C address of the sensor. (default is 0x40)
    @param device_address Path to the device file. (ie. "/dev/i2c-1")

    @return Returns INA_SUCCESS or INA_FAILURE.
*/
int inaI2CInit(LecaINA219* ina, uint8_t i2cAddress, const char* device_address);

/*
    Terminates the communication with the sensor, closes its file descriptor and frees memory occupied by struct instance.

    @param ina Instance of LecaINA219 struct.

    @return Returns INA_SUCCESS or INA_FAILURE.
*/
int inaTerminate(LecaINA219* ina);


/*
    Configures and calibrates the sensor.

    @param ina Instance of LecaINA219 struct.
    @param voltageRange Whether the sensor will work in 16V or 32V mode. This setting has a CONFIG_VOLTAGE prefix.
    @param gain Voltage over the shunt resistor. This setting has a CONFIG_GAIN prefix.
    @param badc Precision of bus voltage ADC conversion. This setting has a CONFIG_ADC prefix.
    @param sadc Precision of shunt voltage ADC conversion. This setting has a CONFIG_ADC prefix.
    @param mode Mode of operation. This setting has a CONFIG_MODE prefix.

    @return Returns INA_SUCCESS or INA_FAILURE.
*/
int inaConfigure(LecaINA219* ina, uint8_t voltageRange, uint8_t gain, uint8_t badc, uint8_t sadc, uint8_t mode);

/*
    Reads the current register and calculates current in amperes.

    @param ina Instance of LecaINA219 struct.
    @param destination Address where the result will be stored.

    @return Returns INA_SUCCESS or INA_FAILURE.
*/
int inaReadCurrent_A(LecaINA219* ina, float* destination);

/*
    Reads the current register and calculates current in milliamperes.

    @param ina Instance of LecaINA219 struct.
    @param destination Address where the result will be stored.

    @return Returns INA_SUCCESS or INA_FAILURE.
*/
int inaReadCurrent_mA(LecaINA219* ina, float* destination);

/*
    Reads the bus voltage register and calculates voltage in volts.

    @param ina Instance of LecaINA219 struct.
    @param destination Address where the result will be stored.

    @return Returns INA_SUCCESS or INA_FAILURE.
*/
int inaReadBusVoltage_V(LecaINA219* ina, float* destination);

/*
    Reads the bus voltage register and calculates voltage in millivolts.

    @param ina Instance of LecaINA219 struct.
    @param destination Address where the result will be stored.

    @return Returns INA_SUCCESS or INA_FAILURE.
*/
int inaReadBusVoltage_mV(LecaINA219* ina, float* destination);

/*
    Reads the power register and calculates power in watts.

    @param ina Instance of LecaINA219 struct.
    @param destination Address where the result will be stored.

    @return Returns INA_SUCCESS or INA_FAILURE.
*/
int inaReadPower_W(LecaINA219* ina, float* destination);

/*
    Reads the power register and calculates power in milliwatts.

    @param ina Instance of LecaINA219 struct.
    @param destination Address where the result will be stored.

    @return Returns INA_SUCCESS or INA_FAILURE.
*/
int inaReadPower_mW(LecaINA219* ina, float* destination);

/*   
    Reads the shunt voltage register and calculates shunt voltage in millivolts.

    @param ina Instance of LecaINA219 struct.
    @param destination Address where the result will be stored.

    @return Returns INA_SUCCESS or INA_FAILURE. 
*/
int inaReadShuntVoltage_mV(LecaINA219* ina, float* destination);

/*
    Triggers the sensor to capture data specified by the *receipt* parameter.

    @param ina Instance of LecaINA219 struct.
    @param receipt Binary number calculated by using the chart specified below.
    @param datapack Address of a datapack struct in which the data will be stored.

    @param Bit0 Get bus voltage (V)
    @param Bit1 Get bus voltage (mV)
    @param Bit2 Get current (A)
    @param Bit3 Get current (mA)
    @param Bit4 Get power (W)
    @param Bit5 Get power (mW)
    @param Bit6 Get shunt voltage (mV)
    @param Bit7 Doesn't matter

    @return Returns INA_SUCCESS or INA_FAILURE.  
*/
int inaGetData(LecaINA219* ina, int receipt, DATAPACK* datapack);

/*
    Prints data stored inside a datapack. Only prints values that were captured by inaGetData().

    @param datapack Address of the datapack.

    @return INA_SUCCESS or INA_FAILURE.
*/
void printDatapack(DATAPACK* datapack);

#endif