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
#include "usbd_cdc_if.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
    BUTTON_ACTION_NONE = 0,
    BUTTON_ACTION_SLOW,
    BUTTON_ACTION_FAST
} PendingButtonAction;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SLOW_BLINK_INTERVAL_MS        2000UL
#define FAST_BLINK_INTERVAL_MS         250UL
#define MIN_ACCELERATION_INTERVAL_MS   100UL
#define DEBOUNCE_DELAY_MS               50UL
#define BUTTON_COMBINATION_WINDOW_MS   150UL

#define SLOWER_BUTTON_PORT GPIOB
#define SLOWER_BUTTON_PIN  GPIO_PIN_0
#define FASTER_BUTTON_PORT GPIOB
#define FASTER_BUTTON_PIN  GPIO_PIN_1
#define BUTTON_PRESSED_LEVEL GPIO_PIN_SET

#define LED_PRIMARY_PORT   GPIOA
#define LED_PRIMARY_PIN    GPIO_PIN_1
#define LED_SECONDARY_PORT GPIOA
#define LED_SECONDARY_PIN  GPIO_PIN_2
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static GPIO_PinState ledState = GPIO_PIN_RESET;
static uint32_t blinkIntervalMs = SLOW_BLINK_INTERVAL_MS;
static uint32_t previousBlinkTimeMs = 0U;
static bool isAccelerating = false;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
static void updateBlinking(void);
static void updateButtons(void);
static void setLedStates(void);
static void enableSlowMode(void);
static void enableFastMode(void);
static void enableAccelerationMode(void);
static void usbPrint(const char *message);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
  setLedStates();

  /* Allow the host enough time to enumerate the USB CDC device. */
  HAL_Delay(1000);
/* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    updateButtons();
    updateBlinking();
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 15;
  RCC_OscInitStruct.PLL.PLLN = 144;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 5;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
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
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_2, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA1 PA2 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* Override the generated no-pull configuration. The buttons are wired
     active-high, so pull-down resistors keep released inputs stable. */
  GPIO_InitStruct.Pin = SLOWER_BUTTON_PIN | FASTER_BUTTON_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
static void usbPrint(const char *message)
{
    /* Debug output is non-critical. Drop the message if USB is unavailable
       or the previous CDC transfer is still in progress. */
    (void)CDC_Transmit_FS((uint8_t *)message, (uint16_t)strlen(message));
}

static void updateBlinking(void)
{
    const uint32_t nowMs = HAL_GetTick();

    if ((nowMs - previousBlinkTimeMs) < blinkIntervalMs)
    {
        return;
    }

    previousBlinkTimeMs = nowMs;
    ledState = (ledState == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
    setLedStates();

    if (!isAccelerating)
    {
        return;
    }

    /* Shorten the period to 80%. Restart after reaching the minimum so the
       acceleration pattern repeats indefinitely. */
    blinkIntervalMs = (blinkIntervalMs * 5UL) / 10UL;

    if (blinkIntervalMs <= MIN_ACCELERATION_INTERVAL_MS)
    {
        blinkIntervalMs = SLOW_BLINK_INTERVAL_MS;
    }
}

static void updateButtons(void)
{
    static bool initialized = false;

    static GPIO_PinState rawSlowerState;
    static GPIO_PinState rawFasterState;
    static GPIO_PinState stableSlowerState;
    static GPIO_PinState stableFasterState;

    static uint32_t slowerChangedAtMs = 0U;
    static uint32_t fasterChangedAtMs = 0U;

    static PendingButtonAction pendingAction = BUTTON_ACTION_NONE;
    static uint32_t pendingActionStartedAtMs = 0U;

    const uint32_t nowMs = HAL_GetTick();
    const GPIO_PinState currentRawSlowerState =
        HAL_GPIO_ReadPin(SLOWER_BUTTON_PORT, SLOWER_BUTTON_PIN);
    const GPIO_PinState currentRawFasterState =
        HAL_GPIO_ReadPin(FASTER_BUTTON_PORT, FASTER_BUTTON_PIN);

    if (!initialized)
    {
        rawSlowerState = currentRawSlowerState;
        rawFasterState = currentRawFasterState;
        stableSlowerState = currentRawSlowerState;
        stableFasterState = currentRawFasterState;
        slowerChangedAtMs = nowMs;
        fasterChangedAtMs = nowMs;
        initialized = true;
        return;
    }

    if (currentRawSlowerState != rawSlowerState)
    {
        rawSlowerState = currentRawSlowerState;
        slowerChangedAtMs = nowMs;
    }

    if (currentRawFasterState != rawFasterState)
    {
        rawFasterState = currentRawFasterState;
        fasterChangedAtMs = nowMs;
    }

    bool slowerPressedEdge = false;
    bool fasterPressedEdge = false;

    if ((rawSlowerState != stableSlowerState) &&
        ((nowMs - slowerChangedAtMs) >= DEBOUNCE_DELAY_MS))
    {
        stableSlowerState = rawSlowerState;
        slowerPressedEdge = (stableSlowerState == BUTTON_PRESSED_LEVEL);
    }

    if ((rawFasterState != stableFasterState) &&
        ((nowMs - fasterChangedAtMs) >= DEBOUNCE_DELAY_MS))
    {
        stableFasterState = rawFasterState;
        fasterPressedEdge = (stableFasterState == BUTTON_PRESSED_LEVEL);
    }

    const bool slowerIsHeld = (stableSlowerState == BUTTON_PRESSED_LEVEL);
    const bool fasterIsHeld = (stableFasterState == BUTTON_PRESSED_LEVEL);

    /* A two-button chord takes priority over either pending single press. */
    if (slowerIsHeld && fasterIsHeld)
    {
        pendingAction = BUTTON_ACTION_NONE;

        if (!isAccelerating)
        {
            enableAccelerationMode();
        }

        return;
    }

    if (slowerPressedEdge)
    {
        pendingAction = BUTTON_ACTION_SLOW;
        pendingActionStartedAtMs = nowMs;
    }
    else if (fasterPressedEdge)
    {
        pendingAction = BUTTON_ACTION_FAST;
        pendingActionStartedAtMs = nowMs;
    }

    if ((pendingAction == BUTTON_ACTION_NONE) ||
        ((nowMs - pendingActionStartedAtMs) < BUTTON_COMBINATION_WINDOW_MS))
    {
        return;
    }

    const PendingButtonAction actionToRun = pendingAction;
    pendingAction = BUTTON_ACTION_NONE;

    if (actionToRun == BUTTON_ACTION_SLOW)
    {
        enableSlowMode();
    }
    else
    {
        enableFastMode();
    }
}

static void setLedStates(void)
{
    HAL_GPIO_WritePin(
        LED_PRIMARY_PORT,
        LED_PRIMARY_PIN,
        ledState
    );

    HAL_GPIO_WritePin(
        LED_SECONDARY_PORT,
        LED_SECONDARY_PIN,
        (ledState == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET
    );
}

static void enableSlowMode(void)
{
    blinkIntervalMs = SLOW_BLINK_INTERVAL_MS;
    isAccelerating = false;
    usbPrint("Slow mode enabled\r\n");
}

static void enableFastMode(void)
{
    blinkIntervalMs = FAST_BLINK_INTERVAL_MS;
    isAccelerating = false;
    usbPrint("Fast mode enabled\r\n");
}

static void enableAccelerationMode(void)
{
    blinkIntervalMs = SLOW_BLINK_INTERVAL_MS;
    isAccelerating = true;
    usbPrint("Acceleration mode enabled\r\n");
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* Disable interrupts and remain here so the debugger can inspect state. */
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
  /* Add project-specific assertion reporting here when needed. */
/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
