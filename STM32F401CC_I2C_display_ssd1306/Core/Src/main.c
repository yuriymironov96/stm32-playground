/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "fonts.h"
#include "ssd1306.h"
#include "ds1307.h"
#include <stdio.h>
#include <string.h>
#include "bme280.h"
#include "usbd_cdc_if.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
    DS1307_Time time;
    s32 temperature_centi_c;
    u32 pressure_pa;
    u32 humidity_q22_10;
    uint8_t rtc_valid;
    uint8_t bme_valid;
} DisplayData;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define RTC_POLL_MS 200U
#define BME_POLL_MS 500U
#define DISPLAY_ROWS 4U
#define DISPLAY_LINE_SIZE 24U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */
/* Bus adapter and Bosch device instance live in bme280_support.c. */
extern struct bme280_t bme280;
extern USBD_HandleTypeDef hUsbDeviceFS;
s8 SPI_routine(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Keep these symbols for the unused Bosch demonstration template, too. */
uint8_t logData[200];
uint16_t logDataLen;

/* USB sends asynchronously: never hand it a stack buffer or overwrite it busy. */
uint8_t digital120_serial_send(const uint8_t *data, const uint16_t len)
{
    static uint8_t tx_buffer[sizeof logData];
    uint8_t sent = 0;
    if (data == NULL || len == 0 || len > sizeof tx_buffer) {
        return 0;
    }

    /* USB disconnect can invalidate pClassData; protect check + submission. */
    uint32_t interrupt_mask = __get_PRIMASK();
    __disable_irq();
    USBD_CDC_HandleTypeDef *cdc = hUsbDeviceFS.pClassData;
    if (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED &&
        cdc != NULL && cdc->TxState == 0) {
        memcpy(tx_buffer, data, len);
        sent = (CDC_Transmit_FS(tx_buffer, len) == USBD_OK);
    }
    __set_PRIMASK(interrupt_mask);
    return sent;
}

static uint8_t BME280_Start(void)
{
    SPI_routine();
    if (bme280_init(&bme280) != 0 || bme280.chip_id != BME280_CHIP_ID ||
        bme280.cal_param.dig_P1 == 0) {
        return 0;
    }

    /* Configure in sleep; the final ctrl_meas write also latches humidity. */
    if (bme280_set_power_mode(BME280_SLEEP_MODE) != 0 ||
        bme280_set_oversamp_humidity(BME280_OVERSAMP_1X) != 0 ||
        bme280_set_oversamp_pressure(BME280_OVERSAMP_1X) != 0 ||
        bme280_set_oversamp_temperature(BME280_OVERSAMP_1X) != 0 ||
        bme280_set_standby_durn(BME280_STANDBY_TIME_250_MS) != 0 ||
        bme280_set_filter(BME280_FILTER_COEFF_OFF) != 0 ||
        bme280_set_power_mode(BME280_NORMAL_MODE) != 0) {
        return 0;
    }

    /* Check actual register contents rather than just SPI transfer success. */
    u8 ctrl_hum = 0, ctrl_meas = 0, config = 0;
    return bme280_read_register(BME280_CTRL_HUMIDITY_REG, &ctrl_hum, 1) == 0 &&
           bme280_read_register(BME280_CTRL_MEAS_REG, &ctrl_meas, 1) == 0 &&
           bme280_read_register(BME280_CONFIG_REG, &config, 1) == 0 &&
           (ctrl_hum & 0x07) == 0x01 && ctrl_meas == 0x27 &&
           (config & 0xFD) == 0x60;
}

static uint8_t BME280_Read(DisplayData *data)
{
    u8 chip_id = 0;
    u32 pressure, humidity;
    s32 temperature;

    /* An unplugged SPI slave may still produce HAL_OK, so check its identity. */
    if (bme280_read_register(BME280_CHIP_ID_REG, &chip_id, 1) != 0 ||
        chip_id != BME280_CHIP_ID ||
        bme280_read_pressure_temperature_humidity(
            &pressure, &temperature, &humidity) != 0) {
        return 0;
    }
    if (temperature < -4000 || temperature > 8500 ||
        pressure < 30000 || pressure > 110000 || humidity > 102400) {
        return 0;
    }
    data->temperature_centi_c = temperature;
    data->pressure_pa = pressure;
    data->humidity_q22_10 = humidity;
    return 1;
}

static void FormatDisplay(const DisplayData *data,
                          char lines[DISPLAY_ROWS][DISPLAY_LINE_SIZE])
{
    if (data->rtc_valid) {
        snprintf(lines[0], DISPLAY_LINE_SIZE, "%02u:%02u:%02u",
                 (unsigned)data->time.hours, (unsigned)data->time.minutes,
                 (unsigned)data->time.seconds);
    } else {
        snprintf(lines[0], DISPLAY_LINE_SIZE, "RTC: ERROR");
    }

    if (!data->bme_valid) {
        snprintf(lines[1], DISPLAY_LINE_SIZE, "T: -- C");
        snprintf(lines[2], DISPLAY_LINE_SIZE, "RH: -- %%RH");
        snprintf(lines[3], DISPLAY_LINE_SIZE, "P: -- hPa");
        return;
    }

    /* Integer formatting avoids needing newlib-nano's optional float printf. */
    s32 t = data->temperature_centi_c;
    s32 tenths = (t < 0 ? t - 5 : t + 5) / 10;
    u32 magnitude = (u32)(tenths < 0 ? -tenths : tenths);
    u32 rh_tenths = (data->humidity_q22_10 * 10U + 512U) / 1024U;
    u32 p_tenths = (data->pressure_pa + 5U) / 10U;
    snprintf(lines[1], DISPLAY_LINE_SIZE, "T: %s%lu.%lu C",
             tenths < 0 ? "-" : "", (unsigned long)(magnitude / 10U),
             (unsigned long)(magnitude % 10U));
    snprintf(lines[2], DISPLAY_LINE_SIZE, "RH: %lu.%lu %%RH",
             (unsigned long)(rh_tenths / 10U), (unsigned long)(rh_tenths % 10U));
    snprintf(lines[3], DISPLAY_LINE_SIZE, "P: %lu.%lu hPa",
             (unsigned long)(p_tenths / 10U), (unsigned long)(p_tenths % 10U));
}

static void PublishDisplay(const DisplayData *data, uint8_t oled_ready)
{
    char lines[DISPLAY_ROWS][DISPLAY_LINE_SIZE];
    static char previous[DISPLAY_ROWS][DISPLAY_LINE_SIZE];
    uint8_t changed = 0;
    FormatDisplay(data, lines);
    for (uint8_t row = 0; row < DISPLAY_ROWS; ++row) {
        if (strcmp(lines[row], previous[row]) != 0) {
            changed = 1;
        }
    }

    if (oled_ready && changed) {
        SSD1306_Fill(SSD1306_COLOR_BLACK);
        for (uint8_t row = 0; row < DISPLAY_ROWS; ++row) {
            SSD1306_GotoXY(0, row * 16U);
            SSD1306_Puts(lines[row], &Font_7x10, SSD1306_COLOR_WHITE);
            strcpy(previous[row], lines[row]);
        }
        /* Draw a degree mark in the space before C; the font is ASCII-only. */
        if (data->bme_valid) {
            SSD1306_DrawCircle((strlen(lines[1]) - 2U) * 7U + 3U,
                               18, 1, SSD1306_COLOR_WHITE);
        }
        SSD1306_UpdateScreen();
    }

    /* Latest snapshot wins while USB is disconnected/busy; never block on it. */
    int length = snprintf((char *)logData, sizeof logData,
                          "%s | %s | %s | %s | BME:%s OLED:%s\r\n",
                          lines[0], lines[1], lines[2], lines[3],
                          data->bme_valid ? "OK" : "NOT READY",
                          oled_ready ? "OK" : "ERROR");
    logDataLen = length > 0 && length < (int)sizeof logData ? (uint16_t)length : 0;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USB_DEVICE_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  DisplayData data = {0};
  uint8_t oled_ready = SSD1306_Init();
  uint8_t bme_ready = BME280_Start();
  /* Preserve the battery-backed RTC; setting a test time on every boot loses it. */
  DS1307_Time initTime = {
      .hours = 23,
      .minutes = 59,
      .seconds = 50
  };
  DS1307_SetTime(&hi2c1, &initTime);
  data.rtc_valid = (DS1307_ReadTime(&hi2c1, &data.time) == HAL_OK);
  uint32_t last_rtc_poll = HAL_GetTick();
  uint32_t last_bme_poll = last_rtc_poll;
  PublishDisplay(&data, oled_ready);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now = HAL_GetTick();
    uint8_t publish = 0;

    if ((uint32_t)(now - last_rtc_poll) >= RTC_POLL_MS) {
        uint8_t was_valid = data.rtc_valid;
        DS1307_Time previous_time = data.time;
        last_rtc_poll = now;
        data.rtc_valid = (DS1307_ReadTime(&hi2c1, &data.time) == HAL_OK);
        publish = data.rtc_valid != was_valid ||
                  (data.rtc_valid && (data.time.seconds != previous_time.seconds ||
                   data.time.minutes != previous_time.minutes ||
                   data.time.hours != previous_time.hours));
    }

    if ((uint32_t)(now - last_bme_poll) >= BME_POLL_MS) {
        last_bme_poll = now;
        /* Normal mode converts autonomously; no conversion delay in this loop. */
        data.bme_valid = bme_ready && BME280_Read(&data);
        publish = 1;
    }

    if (publish) {
        PublishDisplay(&data, oled_ready);
    }
    if (logDataLen != 0 && digital120_serial_send(logData, logDataLen)) {
        logDataLen = 0;
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);

  /*Configure GPIO pin : PB13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
