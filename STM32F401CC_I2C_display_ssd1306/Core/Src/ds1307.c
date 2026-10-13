#include "ds1307.h"

#define DS1307_I2C_ADDRESS (0x68 << 1)
#define DS1307_SECONDS_REGISTER 0x00
#define DS1307_MINUTES_REGISTER 0x01
#define DS1307_HOURS_REGISTER 0x02
#define DS1307_TIMEOUT_MS 100

static uint8_t BCD_ToDecimal(uint8_t value)
{
    return (value >> 4) * 10 + (value & 0x0F);
}

static uint8_t BCD_FromDecimal(uint8_t value)
{
	return (value / 10) << 4 | (value % 10);
}

HAL_StatusTypeDef DS1307_ReadTime(I2C_HandleTypeDef *hi2c, DS1307_Time *time)
{
    uint8_t registers[3];
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(
        hi2c, DS1307_I2C_ADDRESS, DS1307_SECONDS_REGISTER,
        I2C_MEMADD_SIZE_8BIT, registers, sizeof registers, DS1307_TIMEOUT_MS);

    if (status != HAL_OK) {
        return status;
    }

    /* CH (seconds bit 7) stops the clock; hours bit 6 selects 12-hour mode. */
    if ((registers[0] & 0x80) || (registers[2] & 0x40)) {
        return HAL_ERROR;
    }

    time->seconds = BCD_ToDecimal(registers[0] & 0x7F);
    time->minutes = BCD_ToDecimal(registers[1] & 0x7F);
    time->hours = BCD_ToDecimal(registers[2] & 0x3F);
    return HAL_OK;
}

HAL_StatusTypeDef DS1307_SetTime(I2C_HandleTypeDef *hi2c, DS1307_Time *time) {
	if (time->seconds > 59 || time->minutes > 59 || time->hours > 23) {
		return HAL_ERROR;
	}
	uint8_t convertedTime[3] = {
			BCD_FromDecimal(time->seconds),
			BCD_FromDecimal(time->minutes),
			BCD_FromDecimal(time->hours)
	};
	HAL_StatusTypeDef status = HAL_I2C_Mem_Write(hi2c, DS1307_I2C_ADDRESS, DS1307_SECONDS_REGISTER, I2C_MEMADD_SIZE_8BIT, convertedTime, sizeof convertedTime, DS1307_TIMEOUT_MS);

	return status;
}
