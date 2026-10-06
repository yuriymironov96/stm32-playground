#ifndef INC_DS1307_H_
#define INC_DS1307_H_

#include "stm32f4xx_hal.h"

typedef struct {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
} DS1307_Time;

/* Read a running clock in 24-hour mode. Does not set or start the clock.
 * Returns the HAL read status, or HAL_ERROR for a stopped clock/12-hour mode.
 * The output is updated only on success.
 */
HAL_StatusTypeDef DS1307_ReadTime(I2C_HandleTypeDef *hi2c, DS1307_Time *time);

#endif
