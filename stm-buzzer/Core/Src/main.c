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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <math.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef struct {
	uint16_t frequencyHz;
	uint16_t divider;
} Note;

typedef enum {
	PLAYER_SOUND, PLAYER_GAP, PLAYER_REPEAT_PAUSE
} PlayerState;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define SINE_TABLE_SIZE       256U
#define SAMPLE_RATE_HZ        50000U
#define NOKIA_TEMPO_BPM       180U
#define NOTE_ON_PERCENT       90U
#define REPEAT_PAUSE_MS       1500U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

#define NOTE_GS5   830U
#define NOTE_G5   784U
#define NOTE_F5   698U
#define NOTE_E5   659U
#define NOTE_D5   587U
#define NOTE_FS4  370U
#define NOTE_GS4  415U
#define NOTE_CS5  554U
#define NOTE_B4   494U
#define NOTE_D4   294U
#define NOTE_E4   330U
#define NOTE_A4   440U
#define NOTE_CS4  277U
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* USER CODE BEGIN PV */

static uint16_t sineTable[SINE_TABLE_SIZE];

static volatile uint32_t phaseAccumulator = 0U;
static volatile uint32_t phaseStep = 0U;
static volatile uint8_t toneEnabled = 0U;

// plays Nokia tune until it receives btn press
static volatile bool isStandardTune = true;

static PlayerState playerState = PLAYER_REPEAT_PAUSE;
static uint32_t currentNoteIndex = 0U;
static uint32_t stateStartedAtMs = 0U;
static uint32_t stateDurationMs = 0U;
static uint32_t currentGapDurationMs = 0U;

static const Note nokiaTune[] = { { NOTE_E5, 8U }, { NOTE_D5, 8U }, { NOTE_FS4,
		4U }, { NOTE_GS4, 4U },

{ NOTE_CS5, 8U }, { NOTE_B4, 8U }, { NOTE_D4, 4U }, { NOTE_E4, 4U },

{ NOTE_B4, 8U }, { NOTE_A4, 8U }, { NOTE_CS4, 4U }, { NOTE_E4, 4U },

{ NOTE_A4, 2U } };

static const uint32_t nokiaTuneLength = sizeof(nokiaTune)
		/ sizeof(nokiaTune[0]);
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_USB_OTG_FS_PCD_Init(void);
/* USER CODE BEGIN PFP */

static void SineTable_Init(void);
static void Tone_SetFrequency(uint16_t frequencyHz);
static void Tone_Stop(void);
static void NokiaPlayer_Start(void);
static void NokiaPlayer_StartCurrentNote(uint32_t nowMs);
static void NokiaPlayer_Update(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void SineTable_Init(void) {
	const float twoPi = 6.28318530718f;

	for (uint32_t index = 0U; index < SINE_TABLE_SIZE; index++) {
		float angle = twoPi * (float) index / (float) SINE_TABLE_SIZE;
		float sample = 127.5f + 127.5f * sinf(angle);

		sineTable[index] = (uint16_t) sample;
	}
}

static void Tone_SetFrequency(uint16_t frequencyHz) {
	phaseStep = (uint32_t) (((uint64_t) frequencyHz << 32) / SAMPLE_RATE_HZ);

	toneEnabled = 1U;
}

static void Tone_Stop(void) {
	toneEnabled = 0U;
	phaseStep = 0U;

	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0U);
}

static void NokiaPlayer_StartCurrentNote(uint32_t nowMs) {
	const uint32_t wholeNoteMs = (60000U * 4U) / NOKIA_TEMPO_BPM;

	uint32_t noteDurationMs = wholeNoteMs / nokiaTune[currentNoteIndex].divider;

	uint32_t soundDurationMs = (noteDurationMs * NOTE_ON_PERCENT) / 100U;

	currentGapDurationMs = noteDurationMs - soundDurationMs;

	Tone_SetFrequency(nokiaTune[currentNoteIndex].frequencyHz);

	playerState = PLAYER_SOUND;
	stateStartedAtMs = nowMs;
	stateDurationMs = soundDurationMs;
}

static void NokiaPlayer_Start(void) {
	currentNoteIndex = 0U;
	NokiaPlayer_StartCurrentNote(HAL_GetTick());
}

static void NokiaPlayer_Update(void) {
	uint32_t nowMs = HAL_GetTick();

	if ((uint32_t) (nowMs - stateStartedAtMs) < stateDurationMs) {
		return;
	}

	switch (playerState) {
	case PLAYER_SOUND:
		Tone_Stop();
		playerState = PLAYER_GAP;
		stateStartedAtMs = nowMs;
		stateDurationMs = currentGapDurationMs;
		break;

	case PLAYER_GAP:
		currentNoteIndex++;

		if (currentNoteIndex < nokiaTuneLength) {
			NokiaPlayer_StartCurrentNote(nowMs);
		} else {
			Tone_Stop();
			playerState = PLAYER_REPEAT_PAUSE;
			stateStartedAtMs = nowMs;
			stateDurationMs = REPEAT_PAUSE_MS;
		}
		break;

	case PLAYER_REPEAT_PAUSE:
	default:
		currentNoteIndex = 0U;
		NokiaPlayer_StartCurrentNote(nowMs);
		break;
	}
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

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
	MX_TIM2_Init();
	MX_TIM3_Init();
	MX_USB_OTG_FS_PCD_Init();
	/* USER CODE BEGIN 2 */

	SineTable_Init();

	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
	HAL_TIM_Base_Start_IT(&htim3);

	Tone_Stop();
	NokiaPlayer_Start();
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */

	while (1) {
		if (isStandardTune) {
			NokiaPlayer_Update();
		}

		/* Other non-blocking application work can run here. */
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */

	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

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
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void) {

	/* USER CODE BEGIN TIM2_Init 0 */

	/* USER CODE END TIM2_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };

	/* USER CODE BEGIN TIM2_Init 1 */

	/* USER CODE END TIM2_Init 1 */
	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 1;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = 255;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM2_Init 2 */

	/* USER CODE END TIM2_Init 2 */
	HAL_TIM_MspPostInit(&htim2);

}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init(void) {

	/* USER CODE BEGIN TIM3_Init 0 */

	/* USER CODE END TIM3_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM3_Init 1 */

	/* USER CODE END TIM3_Init 1 */
	htim3.Instance = TIM3;
	htim3.Init.Prescaler = 4 - 1;
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim3.Init.Period = 420 - 1;
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM3_Init 2 */

	/* USER CODE END TIM3_Init 2 */

}

/**
 * @brief USB_OTG_FS Initialization Function
 * @param None
 * @retval None
 */
static void MX_USB_OTG_FS_PCD_Init(void) {

	/* USER CODE BEGIN USB_OTG_FS_Init 0 */

	/* USER CODE END USB_OTG_FS_Init 0 */

	/* USER CODE BEGIN USB_OTG_FS_Init 1 */

	/* USER CODE END USB_OTG_FS_Init 1 */
	hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
	hpcd_USB_OTG_FS.Init.dev_endpoints = 4;
	hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
	hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
	hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
	hpcd_USB_OTG_FS.Init.Sof_enable = DISABLE;
	hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
	hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
	hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;
	hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
	if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USB_OTG_FS_Init 2 */

	/* USER CODE END USB_OTG_FS_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();

	/*Configure GPIO pins : PA1 PA2 PA3 PA4 */
	GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI1_IRQn);

	HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI2_IRQn);

	HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI3_IRQn);

	HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI4_IRQn);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    // we are reacting to some event. To avoid confusion,
    // we stop playing any note, and if any button is really pressed -
    // we start playing its note
	Tone_Stop();
	isStandardTune = false;
	if (GPIO_Pin == GPIO_PIN_1 && HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET) {
		Tone_SetFrequency(NOTE_D5);
	} else if (GPIO_Pin == GPIO_PIN_2 && HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_RESET) {
		Tone_SetFrequency(NOTE_F5);
	} else if (GPIO_Pin == GPIO_PIN_3 && HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3) == GPIO_PIN_RESET) {
		Tone_SetFrequency(NOTE_G5);
	} else if (GPIO_Pin == GPIO_PIN_4 && HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET) {
		Tone_SetFrequency(NOTE_GS5);
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM3) {
		if (toneEnabled != 0U) {
			phaseAccumulator += phaseStep;

			uint8_t tableIndex = (uint8_t) (phaseAccumulator >> 24);

			__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, sineTable[tableIndex]);
		} else {
			__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0U);
		}
	}
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
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
