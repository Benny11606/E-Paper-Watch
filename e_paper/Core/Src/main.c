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
#include "EPD_Test.h"
#include "EPD_1in54_V2.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim7;
TIM_HandleTypeDef htim16;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
UBYTE BlackImage[5000];
PAINT_TIME sPaint_time;
sFONT font;
int fontWidth;
int fontHeight;
uint32_t PrevB1Press;
uint32_t PrevB2Press;
int refreshFlag;
int adjMode;
int adjPos;

typedef enum {
	MON,
	TUE,
	WED,
	THU,
	FRI,
	SAT,
	SUN
} dayName;

int todayName = WED;
int day = 22;
int month = 7;
int year = 2026;

int alarmHour;
int alarmMinute;
int alarmToggle;
int alarmPlay;
int alarmStopped;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM6_Init(void);
static void MX_TIM7_Init(void);
static void MX_TIM16_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){

	if (htim == &htim6) {
		if (HAL_GPIO_ReadPin(GPIOB, B1_Pin) == GPIO_PIN_SET) {
			HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
			adjMode = adjMode ^ 1;
			adjPos = 0;
			Paint_DrawRectangle(0, 70 + fontHeight,
								  fontWidth * 2 - 3, 75 + fontHeight,
								  BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
			refreshFlag = 1;
			HAL_TIM_Base_Stop_IT(htim);
		}
	} else if (htim == &htim7) {
		sPaint_time.Sec = sPaint_time.Sec + 1;
		if (sPaint_time.Sec == 60) {
			sPaint_time.Min = sPaint_time.Min + 1;
			sPaint_time.Sec = 0;
			if (sPaint_time.Min == 60) {
				sPaint_time.Hour =  sPaint_time.Hour + 1;
				sPaint_time.Min = 0;
				if (sPaint_time.Hour >= 24) {
					sPaint_time.Hour = 0;
					sPaint_time.Min = 0;
					sPaint_time.Sec = 0;

					todayName++;
					if (todayName > SUN) {
						todayName = MON;
					}

					day++;
				}
			}
		}
		Paint_ClearWindows(0, 74, 0 + fontWidth * 7, 74 + fontHeight, WHITE);
		Paint_DrawTime(0, 74, &sPaint_time, &font, WHITE, BLACK);

		refreshFlag = 1;
	} else if (htim == &htim16) {
		HAL_GPIO_TogglePin(GPIOA, BUZZER_Pin);
	}

}

int maxDay() {
	if (month == 9 || month == 4 || month == 6 || month == 11) {
		return 30;
	} else if (month == 2) {
		if (year % 4 == 0 && (year % 100 != 0 || (year % 100 == 0 && year % 400 == 0))) {
			return 29;
		}

		return 28;
	} else {
		return 31;
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (alarmPlay) {
		HAL_TIM_Base_Stop_IT(&htim16);
		alarmPlay = 0;
		alarmStopped = 1;
	}

	if (GPIO_Pin == B2_Pin) {
		if ((HAL_GetTick() - PrevB2Press) > 500 && HAL_GPIO_ReadPin(GPIOB, B2_Pin) == GPIO_PIN_SET && adjMode) {
			PrevB2Press = HAL_GetTick();

			switch (adjPos) {
			case 0:
				sPaint_time.Hour = sPaint_time.Hour + 1;

				if (sPaint_time.Hour >= 24) {
					sPaint_time.Hour = 0;
				}
				break;
			case 1:
				sPaint_time.Min = sPaint_time.Min + 1;

				if (sPaint_time.Min >= 60) {
					sPaint_time.Min = 0;
				}
				break;
			case 2:
				sPaint_time.Sec = sPaint_time.Sec + 1;

				if (sPaint_time.Sec >= 60) {
					sPaint_time.Sec = 0;
				}
				break;
			case 3:
				todayName++;

				if (todayName > SUN) {
					todayName = MON;
				}
				break;
			case 4:
				month++;

				if (month > 12) {
					month = 1;
				}
				break;
			case 5:
				day++;

				if (day > maxDay()) {
					day = 1;
				}
				break;
			case 6:
				year++;

				if (year > 2100) {
					year = 2000;
				}
				break;
			case 7:
				alarmToggle ^= 1;
				break;
			case 8:
				alarmHour++;

				if (alarmHour > 24) {
					alarmHour = 1;
				}
				break;
			case 9:
				alarmMinute++;

				if (alarmMinute >= 60) {
					alarmMinute = 0;
				}
				break;
			default:
			}
		}
	}

	if (GPIO_Pin == B1_Pin) {
		if ((HAL_GetTick() - PrevB1Press) > 500 && HAL_GPIO_ReadPin(GPIOB, B1_Pin) == GPIO_PIN_SET) {
			PrevB1Press = HAL_GetTick();

			if (adjMode) {
				adjPos++;
				if (adjPos > 9) {
					adjPos = 0;
				}
			}

			HAL_TIM_Base_Stop_IT(&htim6);
			HAL_TIM_Base_Start_IT(&htim6);
		}
	}
}

const char* dayString(dayName day) {
	switch(day) {
	case 0:
		return "MON";
	case 1:
		return "TUE";
	case 2:
		return "WED";
	case 3:
		return "THU";
	case 4:
		return "FRI";
	case 5:
		return "SAT";
	case 6:
		return "SUN";
	default:
		return "";
	}
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
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_TIM6_Init();
  MX_TIM7_Init();
  MX_TIM16_Init();
  /* USER CODE BEGIN 2 */

  // 1. Init hardware and display
  DEV_Module_Init();
  EPD_1IN54_V2_Init();
  EPD_1IN54_V2_Clear();
  DEV_Delay_ms(500);

  font = Consolas26;
  fontWidth = font.Width;
  fontHeight = font.Height;

  Paint_NewImage(BlackImage, EPD_1IN54_V2_WIDTH, EPD_1IN54_V2_HEIGHT, 270, WHITE);

  EPD_1IN54_V2_Init_Partial();
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);

  sPaint_time.Hour = 12;
  sPaint_time.Min = 34;
  sPaint_time.Sec = 56;

  alarmHour = 12;
  alarmMinute = 36;

  Paint_DrawString_EN(0, 0, dayString(todayName), &Font24, BLACK, WHITE);
  Paint_DrawNum(0, Font24.Height, month / 10, &Font24, WHITE, BLACK);
  Paint_DrawNum(Font24.Width, Font24.Height, month % 10, &Font24, WHITE, BLACK);
  Paint_DrawChar(Font24.Width * 2, Font24.Height, '-', &Font24, BLACK, WHITE);
  Paint_DrawNum(Font24.Width * 3, Font24.Height, day / 10, &Font24, WHITE, BLACK);
  Paint_DrawNum(Font24.Width * 4, Font24.Height, day % 10, &Font24, WHITE, BLACK);
  Paint_DrawChar(Font24.Width * 5, Font24.Height, '-', &Font24, BLACK, WHITE);
  Paint_DrawNum(Font24.Width * 6, Font24.Height, year, &Font24, WHITE, BLACK);

  Paint_DrawString_EN(0, EPD_1IN54_V2_HEIGHT - Font24.Height * 2, "ALARM", &Font24, BLACK, WHITE);
  Paint_DrawString_EN(Font24.Width * 6, EPD_1IN54_V2_HEIGHT - Font24.Height * 2, "OFF", &Font24, BLACK, WHITE);
  Paint_DrawNum(0, EPD_1IN54_V2_HEIGHT - Font24.Height, alarmHour / 10, &Font24, WHITE, BLACK);
  Paint_DrawNum(Font24.Width, EPD_1IN54_V2_HEIGHT - Font24.Height, alarmHour % 10, &Font24, WHITE, BLACK);
  Paint_DrawChar(Font24.Width * 2, EPD_1IN54_V2_HEIGHT - Font24.Height, ':', &Font24, BLACK, WHITE);
  Paint_DrawNum(Font24.Width * 3, EPD_1IN54_V2_HEIGHT - Font24.Height, alarmMinute / 10, &Font24, WHITE, BLACK);
  Paint_DrawNum(Font24.Width * 4, EPD_1IN54_V2_HEIGHT - Font24.Height, alarmMinute % 10, &Font24, WHITE, BLACK);

  Paint_DrawTime(0, 74, &sPaint_time, &font, WHITE, BLACK);
  EPD_1IN54_V2_DisplayPart(BlackImage);

  HAL_TIM_Base_Start_IT(&htim7);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if (refreshFlag) {
		  if (adjMode) {
			  if (adjPos < 3) {
				  Paint_ClearWindows(Font24.Width*3, EPD_1IN54_V2_HEIGHT - Font24.Height, Font24.Width*5, EPD_1IN54_V2_HEIGHT, WHITE);
				  Paint_DrawNum(Font24.Width * 3, EPD_1IN54_V2_HEIGHT - Font24.Height, alarmMinute / 10, &Font24, WHITE, BLACK);
				  Paint_DrawNum(Font24.Width * 4, EPD_1IN54_V2_HEIGHT - Font24.Height, alarmMinute % 10, &Font24, WHITE, BLACK);
				  Paint_DrawRectangle(fontWidth * 2 * (adjPos) + (fontWidth / 2) * adjPos, 70 + fontHeight,
						  fontWidth * 2 * (adjPos + 1) + (fontWidth / 2) * adjPos - 3, 75 + fontHeight,
						  BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
			  } else {
				  switch(adjPos) {
				  case 3:
					  Paint_DrawString_EN(0, 0, dayString(todayName), &Font24, WHITE, BLACK);
					  break;
				  case 4:
					  Paint_ClearWindows(0, 0, Font24.Width*3, Font24.Height, WHITE);
					  Paint_DrawString_EN(0, 0, dayString(todayName), &Font24, BLACK, WHITE);
					  Paint_DrawNum(0, Font24.Height, month / 10, &Font24, BLACK, WHITE);
					  Paint_DrawNum(Font24.Width, Font24.Height, month % 10, &Font24, BLACK, WHITE);
					  break;
				  case 5:
					  Paint_ClearWindows(0, Font24.Height, Font24.Width*2, Font24.Height*2, WHITE);
					  Paint_DrawNum(0, Font24.Height, month / 10, &Font24, WHITE, BLACK);
					  Paint_DrawNum(Font24.Width, Font24.Height, month % 10, &Font24, WHITE, BLACK);
					  Paint_DrawNum(Font24.Width * 3, Font24.Height, day / 10, &Font24, BLACK, WHITE);
					  Paint_DrawNum(Font24.Width * 4, Font24.Height, day % 10, &Font24, BLACK, WHITE);
					  break;
				  case 6:
					  Paint_ClearWindows(Font24.Width*3, Font24.Height, Font24.Width*5, Font24.Height*2, WHITE);
					  Paint_DrawNum(Font24.Width * 3, Font24.Height, day / 10, &Font24, WHITE, BLACK);
					  Paint_DrawNum(Font24.Width * 4, Font24.Height, day % 10, &Font24, WHITE, BLACK);
					  Paint_DrawNum(Font24.Width * 6, Font24.Height, year, &Font24, BLACK, WHITE);
					  break;
				  case 7:
					  Paint_ClearWindows(Font24.Width*6, Font24.Height, Font24.Width*10, Font24.Height*2, WHITE);
					  Paint_DrawNum(Font24.Width * 6, Font24.Height, year, &Font24, WHITE, BLACK);
					  Paint_DrawString_EN(Font24.Width * 6, EPD_1IN54_V2_HEIGHT - Font24.Height * 2, (alarmToggle == 0) ? "OFF" : "ON ", &Font24, WHITE, BLACK);
					  break;
				  case 8:
					  Paint_ClearWindows(Font24.Width*6, EPD_1IN54_V2_HEIGHT - Font24.Height * 2, Font24.Width*9, EPD_1IN54_V2_HEIGHT - Font24.Height, WHITE);
					  Paint_DrawString_EN(Font24.Width * 6, EPD_1IN54_V2_HEIGHT - Font24.Height * 2, (alarmToggle == 0) ? "OFF" : "ON ", &Font24, BLACK, WHITE);
					  Paint_DrawNum(0, EPD_1IN54_V2_HEIGHT - Font24.Height, alarmHour / 10, &Font24, BLACK, WHITE);
					  Paint_DrawNum(Font24.Width, EPD_1IN54_V2_HEIGHT - Font24.Height, alarmHour % 10, &Font24, BLACK, WHITE);
					  break;
				  case 9:
					  Paint_ClearWindows(0, EPD_1IN54_V2_HEIGHT - Font24.Height, Font24.Width*2, EPD_1IN54_V2_HEIGHT, WHITE);
					  Paint_DrawNum(0, EPD_1IN54_V2_HEIGHT - Font24.Height, alarmHour / 10, &Font24, WHITE, BLACK);
					  Paint_DrawNum(Font24.Width, EPD_1IN54_V2_HEIGHT - Font24.Height, alarmHour % 10, &Font24, WHITE, BLACK);
					  Paint_DrawNum(Font24.Width * 3, EPD_1IN54_V2_HEIGHT - Font24.Height, alarmMinute / 10, &Font24, BLACK, WHITE);
					  Paint_DrawNum(Font24.Width * 4, EPD_1IN54_V2_HEIGHT - Font24.Height, alarmMinute % 10, &Font24, BLACK, WHITE);
				  default:
				  }

			  }
		  } else {
			  if (day > maxDay()) {
				  day = 1;
				  month++;

				  if (month > 12) {
					  month = 1;
					  year++;
				  }
			  }

			  if (alarmToggle && !alarmStopped && !alarmPlay
				&& sPaint_time.Hour == alarmHour && sPaint_time.Min == alarmMinute) {
				  HAL_TIM_Base_Start_IT(&htim16);
				  alarmPlay = 1;
			  }

			  if (alarmStopped && sPaint_time.Hour == alarmHour && sPaint_time.Min == alarmMinute + 1) {
				  alarmStopped = 0;
			  }

			  Paint_ClearWindows(0, 0, Font24.Width*10, Font24.Height*2, WHITE);
			  Paint_DrawString_EN(0, 0, dayString(todayName), &Font24, BLACK, WHITE);
			  Paint_DrawNum(0, Font24.Height, month / 10, &Font24, WHITE, BLACK);
			  Paint_DrawNum(Font24.Width, Font24.Height, month % 10, &Font24, WHITE, BLACK);
			  Paint_DrawChar(Font24.Width * 2, Font24.Height, '-', &Font24, BLACK, WHITE);
			  Paint_DrawNum(Font24.Width * 3, Font24.Height, day / 10, &Font24, WHITE, BLACK);
			  Paint_DrawNum(Font24.Width * 4, Font24.Height, day % 10, &Font24, WHITE, BLACK);
			  Paint_DrawChar(Font24.Width * 5, Font24.Height, '-', &Font24, BLACK, WHITE);
			  Paint_DrawNum(Font24.Width * 6, Font24.Height, year, &Font24, WHITE, BLACK);

			  Paint_ClearWindows(0, EPD_1IN54_V2_HEIGHT - Font24.Height * 2, Font24.Width*9, EPD_1IN54_V2_HEIGHT, WHITE);
			  Paint_DrawString_EN(0, EPD_1IN54_V2_HEIGHT - Font24.Height * 2, "ALARM", &Font24, BLACK, WHITE);
			  Paint_DrawString_EN(Font24.Width * 6, EPD_1IN54_V2_HEIGHT - Font24.Height * 2, (alarmToggle == 0) ? "OFF" : "ON ", &Font24, BLACK, WHITE);
			  Paint_DrawNum(0, EPD_1IN54_V2_HEIGHT - Font24.Height, alarmHour / 10, &Font24, WHITE, BLACK);
			  Paint_DrawNum(Font24.Width, EPD_1IN54_V2_HEIGHT - Font24.Height, alarmHour % 10, &Font24, WHITE, BLACK);
			  Paint_DrawChar(Font24.Width * 2, EPD_1IN54_V2_HEIGHT - Font24.Height, ':', &Font24, BLACK, WHITE);
			  Paint_DrawNum(Font24.Width * 3, EPD_1IN54_V2_HEIGHT - Font24.Height, alarmMinute / 10, &Font24, WHITE, BLACK);
			  Paint_DrawNum(Font24.Width * 4, EPD_1IN54_V2_HEIGHT - Font24.Height, alarmMinute % 10, &Font24, WHITE, BLACK);
		  }

		  EPD_1IN54_V2_DisplayPart(BlackImage);
		  refreshFlag = 0;
	  }

	  HAL_SuspendTick();
	  HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON,PWR_SLEEPENTRY_WFI);
	  HAL_ResumeTick();
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 16;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
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
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 2048;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 15624;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}

/**
  * @brief TIM7 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM7_Init(void)
{

  /* USER CODE BEGIN TIM7_Init 0 */

  /* USER CODE END TIM7_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM7_Init 1 */

  /* USER CODE END TIM7_Init 1 */
  htim7.Instance = TIM7;
  htim7.Init.Prescaler = 2048;
  htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim7.Init.Period = 15624;
  htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM7_Init 2 */

  /* USER CODE END TIM7_Init 2 */

}

/**
  * @brief TIM16 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM16_Init(void)
{

  /* USER CODE BEGIN TIM16_Init 0 */

  /* USER CODE END TIM16_Init 0 */

  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM16_Init 1 */

  /* USER CODE END TIM16_Init 1 */
  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 0;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 65535;
  htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim16.Init.RepetitionCounter = 0;
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 50000;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim16, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim16, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM16_Init 2 */

  /* USER CODE END TIM16_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, RST_Pin|DC_Pin|CS_Pin|BUZZER_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : RST_Pin DC_Pin CS_Pin BUZZER_Pin */
  GPIO_InitStruct.Pin = RST_Pin|DC_Pin|CS_Pin|BUZZER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : BUSY_Pin */
  GPIO_InitStruct.Pin = BUSY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(BUSY_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : B1_Pin B2_Pin */
  GPIO_InitStruct.Pin = B1_Pin|B2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LD3_Pin */
  GPIO_InitStruct.Pin = LD3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD3_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

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
