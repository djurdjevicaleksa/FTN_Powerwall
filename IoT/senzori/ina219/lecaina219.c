/*
  ------------------------------------------------------------------------------------------------------
  -> File:            lecaina219.c
 
  -> Author:          Aleksa Djurdjevic
  
  -> Last change:     16.4.2024.
  
  -> Description:     
    Source code of a library for controlling and using
    the Texas Instruments INA219 current and voltage sensor.

  -> Additional info: 
    Made as part of a project at Faculty of Technical Sciences in Novi Sad, Serbia.

  -> INA219 data sheet: https://www.ti.com/lit/ds/symlink/ina219.pdf
    -----------------------------------------------------------------------------------------------------
*/

#include "lecaina219.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>

typedef struct ina
{
    uint8_t deviceAddress;
    int fileDescriptor;

    uint8_t voltageRange;
    float shuntResistance;
    float maxExpectedCurrent;
    uint8_t gain;

    float minimumCurrentLSB;
    float currentLSB;
    float powerLSB;
    
}LecaINA219;

static int writeToRegister(LecaINA219* ina, uint8_t reg, uint16_t value);
static int readFromRegister(LecaINA219* ina, uint8_t reg, uint16_t* destination);
static int inaCalibrate(LecaINA219* ina, uint8_t maxBusVoltage, float maxShuntVoltage, float maxExpectedAmps);
static float inaCalculateCurrentLSB(LecaINA219* ina, float maxExpectedAmps, float maxPossibleAmps);
static int inaReadCurrentRegister(LecaINA219* ina, uint16_t* destination);
static int inaReadBusVoltageRegister(LecaINA219* ina, uint16_t* destination);
static int inaReadPowerRegister(LecaINA219* ina, uint16_t* destination);
static int inaReadShuntVoltageRegister(LecaINA219* ina, uint16_t* destination);

LecaINA219* createINA(float shuntResistance, float maxExpectedAmps)
{
    LecaINA219* ina = malloc(sizeof(LecaINA219));
    if(ina == NULL)
    {
        fprintf(stderr, "Failed to create an instance of LecaINA219 struct.\n");
        return INA_FAILURE;
    }

    ina->shuntResistance = shuntResistance;
    ina->maxExpectedCurrent = maxExpectedAmps;
    ina->minimumCurrentLSB = CALIBRATION_CONSTANT / (shuntResistance * CALIBRATION_LSB);

    return ina;
}

int inaI2CInit(LecaINA219* ina, uint8_t i2cAddress, const char* device_address)
{
    int fd = open(device_address, O_RDWR);
    if(fd == -1)
    {
        fprintf(stderr, "Couldn't open device file descriptor.\n");
        return INA_FAILURE;
    }

    ina->fileDescriptor = fd;
    ina->deviceAddress = i2cAddress;

    if(ioctl(fd, I2C_SLAVE, i2cAddress) == -1)
    {
        fprintf(stderr, "Couldn't control the device with address %d.\n", i2cAddress);
        return INA_FAILURE;
    }

    return INA_SUCCESS;
}

int inaTerminate(LecaINA219* ina)
{
    if(close(ina->fileDescriptor) == 0)
    {
        return INA_SUCCESS;
    }

    free(ina);

    return INA_FAILURE;
}

/*
    Writes data to a register. Obscured from the user.

    @param ina Instance of LecaINA219 struct.
    @param reg Register to write to. This setting has a REGISTER prefix.
    @param value Value to write into the register.

    @return Returns INA_SUCCESS or INA_FAILURE.
*/
static int writeToRegister(LecaINA219* ina, uint8_t reg, uint16_t value)
{
    uint8_t buffer[3];
    buffer[0] = reg;
    buffer[1] = value >> 8;
    buffer[2] = value & 0xFF;

    if(write(ina->fileDescriptor, buffer, 3) != 3)
    {
        fprintf(stderr, "Couldn't write data to register %x", reg);
        return INA_FAILURE;
    }

    return INA_SUCCESS;
}

/*
    Reads contents of a register. Obscured from the user.

    @param ina Instance of LecaINA219 struct.
    @param reg Register to read from. This setting has a REGISTER prefix.
    @param destination Address where the contents will be stored.

    @return Returns INA_SUCCESS or INA_FAILURE.
*/
static int readFromRegister(LecaINA219* ina, uint8_t reg, uint16_t* destination)
{
    uint8_t selectedRegister = reg;
    uint8_t buffer[2];

    if(write(ina->fileDescriptor, &selectedRegister, 1) != 1)
    {
        fprintf(stderr, "Couldn't move register pointer to register %x.\n", reg);
        return INA_FAILURE;
    }

    usleep(4);

    if(read(ina->fileDescriptor, buffer, 2) != 2)
    {
        fprintf(stderr, "Couldn't read data from register %x.\n", reg);
        return INA_FAILURE;
    }

    *destination = ((uint16_t)buffer[0] << 8) | buffer[1];

    return INA_SUCCESS;
}

int inaConfigure(LecaINA219* ina, uint8_t voltageRange, uint8_t gain, uint8_t badc, uint8_t sadc, uint8_t mode)
{
    ina->voltageRange = voltageRange;
    ina->gain = gain;

    uint16_t configValue = (voltageRange << 13) | (gain << 11) | (badc << 7) | (sadc << 3) | mode;

    if(writeToRegister(ina, REGISTER_CONFIGURATION, configValue) == INA_FAILURE)
    {
        fprintf(stderr, "Couldn't set value of configuration register.\n");
        return INA_FAILURE;
    }

    inaCalibrate(ina, BUS_VOLTAGES[voltageRange], GAIN[gain], ina->maxExpectedCurrent);
   
    return INA_SUCCESS;
}

/*
    Calibrates the sensor. Used inside inaConfigure(). Cannot be used by the user.

    @param ina Instance of LecaINA219 struct.
    @param maxBusVoltage Maximum voltage allowed on the bus.
    @param maxShuntVoltage Voltage across the shunt resistor. 
    @param maxExpectedAmps Maximum current in amperes the sensor will measure. Lower current means the readings will be more precise.

    @return Returns INA_SUCCESS or INA_FAILURE.
*/
static int inaCalibrate(LecaINA219* ina, uint8_t maxBusVoltage, float maxShuntVoltage, float maxExpectedAmps)
{
	float maxPossibleAmps = maxShuntVoltage / ina->shuntResistance;

    ina->currentLSB = inaCalculateCurrentLSB(ina, maxExpectedAmps, maxPossibleAmps);

	ina->powerLSB = ina->currentLSB * 20.0;

	uint16_t calibration = (uint16_t) trunc(CALIBRATION_CONSTANT / (ina->currentLSB * ina->shuntResistance));
	
    if(writeToRegister(ina, REGISTER_CALIBRATION, calibration) == INA_FAILURE)
    {
        fprintf(stderr, "Couldn't set calibration register's value.\n");
        return INA_FAILURE;
    }

    return INA_SUCCESS;
}

/*
    Helper function to calculate currentLSB to balance precision and current range. Cannot be used by itself.

    @param ina Instance of LecaINA219 struct.
    @param maxExpectedAmps Maximum current in amperes the sensor will measure. Lower current means the readings will be more precise.
    @param maxPossibleAmps Maximum current in amperes the sensor will be able to withstand before causing damage. Internally calculated.

    @return Returns the value for the currentLSB.
*/
static float inaCalculateCurrentLSB(LecaINA219* ina, float maxExpectedAmps, float maxPossibleAmps)
{
	float currentLSB;

	float ballpark = roundf(maxPossibleAmps * 1000.0) / 1000.0;
	if (maxExpectedAmps > ballpark) 
    {
		fprintf(stderr, "Expected current %f A is greater than max possible current %f A", maxExpectedAmps, maxPossibleAmps);
		return INA_FAILURE;
	}

    currentLSB = maxExpectedAmps < maxPossibleAmps ? maxExpectedAmps / CALIBRATION_LSB_DIVIDER : maxPossibleAmps / CALIBRATION_LSB_DIVIDER;

	if (currentLSB < ina->minimumCurrentLSB) 
        currentLSB = ina->minimumCurrentLSB;
	
	return currentLSB;
}

/*
    Reads the current register. Data inside isn't processed and therefore is obscured from the user.

    @param ina Instance of LecaINA219 struct.
    @param destination Address where the result will be stored.

    @return Returns INA_SUCCESS or INA_FAILURE.
*/
static int inaReadCurrentRegister(LecaINA219* ina, uint16_t* destination)
{
    uint16_t rawCurrent;
    readFromRegister(ina, REGISTER_CURRENT, &rawCurrent);

    *destination = rawCurrent;

    return INA_SUCCESS;
}

int inaReadCurrent_mA(LecaINA219* ina, float* destination)
{
    uint16_t rawCurrent;
    
    inaReadCurrentRegister(ina, &rawCurrent);

    int16_t current = (int16_t)rawCurrent;

    if(current > 32767) 
        current -= 65536;
    
    *destination = current * ina->currentLSB * MILLI_MULTIPLIER;

    return INA_SUCCESS;
}

int inaReadCurrent_A(LecaINA219* ina, float* destination)
{
    uint16_t rawCurrent;
    readFromRegister(ina, REGISTER_CURRENT, &rawCurrent);

    int16_t current = (int16_t)rawCurrent;

    if(current > 32767) 
        current -= 65536;
    
    *destination = (float)current * ina->currentLSB;

    return INA_SUCCESS;
}

/*
    Reads the bus voltage register. Data inside isn't processed and therefore is obscured from the user.

    @param ina Instance of LecaINA219 struct.
    @param destination Address where the result will be stored.

    @return Returns INA_SUCCESS or INA_FAILURE. 
*/
static int inaReadBusVoltageRegister(LecaINA219* ina, uint16_t* destination)
{
    uint16_t rawBusVoltage;
    readFromRegister(ina, REGISTER_BUS_VOLTAGE, &rawBusVoltage);
    *destination = rawBusVoltage;

    return INA_SUCCESS;
}

int inaReadBusVoltage_V(LecaINA219* ina, float* destination)
{
    uint16_t rawVoltage;
    inaReadBusVoltageRegister(ina, &rawVoltage);

    float voltage = (float)rawVoltage;
    voltage /= 8;      //can't bit shift a float
    *destination = voltage * 0.004;

    return INA_SUCCESS;
}

int inaReadBusVoltage_mV(LecaINA219* ina, float* destination)
{
    uint16_t rawVoltage;
    inaReadBusVoltageRegister(ina, &rawVoltage);

    float voltage = (float)rawVoltage;
    voltage /= 8;   //can't bit shift a float
    
    *destination = voltage * 0.004 * MILLI_MULTIPLIER;

    return INA_SUCCESS;
}

/*
    Reads the power register. Data inside isn't processed and therefore is obscured from the user.

    @param ina Instance of LecaINA219 struct.
    @param destination Address where the result will be stored.

    @return Returns INA_SUCCESS or INA_FAILURE. 
*/
static int inaReadPowerRegister(LecaINA219* ina, uint16_t* destination)
{
    uint16_t rawPower;

    readFromRegister(ina, REGISTER_POWER, &rawPower);

    *destination = rawPower;

    return INA_SUCCESS;
}

int inaReadPower_W(LecaINA219* ina, float* destination)
{
    uint16_t rawPower;
    inaReadPowerRegister(ina, &rawPower);

    float power;
    power = (float)rawPower;
    power *= ina->powerLSB;
    *destination = power;

    return INA_SUCCESS;
}

int inaReadPower_mW(LecaINA219* ina, float* destination)
{
    uint16_t rawPower;
    inaReadPowerRegister(ina, &rawPower);

    
    float power = (float)rawPower;
    power *= ina->powerLSB;
    *destination = power * MILLI_MULTIPLIER;

    return INA_SUCCESS;
}

/*   
    Reads the shunt voltage register. Data inside isn't processed and therefore is obscured from the user.

    @param ina Instance of LecaINA219 struct.
    @param destination Address where the result will be stored.

    @return Returns INA_SUCCESS or INA_FAILURE. 
*/
static int inaReadShuntVoltageRegister(LecaINA219* ina, uint16_t* destination)
{
    uint16_t rawShuntVoltage;

    readFromRegister(ina, REGISTER_SHUNT_VOLTAGE, &rawShuntVoltage);

    *destination = rawShuntVoltage;

    return INA_SUCCESS;
}

int inaReadShuntVoltage_mV(LecaINA219* ina, float* destination)
{
    uint16_t rawShuntVoltage;
    inaReadShuntVoltageRegister(ina, &rawShuntVoltage);

    int16_t shuntVoltage = (int16_t)rawShuntVoltage;
    *destination = shuntVoltage * SHUNT_VOLTAGE_LSB * SHUNT_MULTIPLIER;

    return INA_SUCCESS;
}

int inaGetData(LecaINA219* ina, int receipt, DATAPACK* datapack)
{
    int i;

    if(receipt & 0b1)
    {
        inaReadBusVoltage_V(ina, &datapack->busVoltage_V);
    }
    else
    {
        datapack->busVoltage_V = -1;
    }

    usleep(5);

    if(receipt >> 1 & 0b1)
    {
        inaReadBusVoltage_mV(ina, &datapack->busVoltage_mV);
    }
    else
    {
        datapack->busVoltage_mV = -1;
    }

    usleep(5);

    if(receipt >> 2 & 0b1)
    {
        inaReadCurrent_A(ina, &datapack->current_A);
    }
    else
    {
        datapack->current_A = -1;
    }

    usleep(5);

    if(receipt >> 3 & 0b1)
    {
        inaReadCurrent_mA(ina, &datapack->current_mA);
    }
    else
    {
        datapack->current_mA = -1;
    }

    usleep(5);

    if(receipt >> 4 & 0b1)
    {
        inaReadPower_W(ina, &datapack->power_W);
    }
    else
    {
        datapack->power_W = -1;
    }

    usleep(5);

    if(receipt >> 5 & 0b1)
    {
        inaReadPower_mW(ina, &datapack->power_mW);
    }
    else
    {
        datapack->power_mW = -1;
    }

    usleep(5);

    if(receipt >> 6 & 0b1)
    {
        inaReadShuntVoltage_mV(ina, &datapack->shuntVoltage_mV);
    }
    else
    {
        datapack->shuntVoltage_mV = -1;
    }

    return INA_SUCCESS;
}

void printDatapack(DATAPACK* datapack)
{
    fprintf(stdout, "---------------------------------------\n");
    
    if(datapack->busVoltage_V != -1)
        fprintf(stdout, "Bus voltage: %.02f V\n", datapack->busVoltage_V);

    if(datapack->busVoltage_mV != -1)
        fprintf(stdout, "Bus voltage: %.02f mV\n", datapack->busVoltage_mV);

    if(datapack->current_A != -1)
        fprintf(stdout, "Current: %.02f A\n", datapack->current_A);

    if(datapack->current_mA != -1)
        fprintf(stdout, "Current: %.02f mA\n", datapack->current_mA);

    if(datapack->power_W != -1)
        fprintf(stdout, "Power: %.02f W\n", datapack->power_W);

    if(datapack->power_mW != -1)
        fprintf(stdout, "Power: %.02f mW\n", datapack->power_mW);

    if(datapack->shuntVoltage_mV != -1)
        fprintf(stdout, "Shunt voltage: %.02f mV\n", datapack->shuntVoltage_mV);

    fprintf(stdout, "---------------------------------------\n");
}