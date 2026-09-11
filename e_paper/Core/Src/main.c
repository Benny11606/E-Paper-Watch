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
RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim16;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
volatile int timerRunning;

// font constants
UBYTE BlackImage[5000];
sFONT timeFont;
sFONT smallFont;
int timeWidth;
int timeHeight;
int smallWidth;
int smallHeight;

// button constants for de-bouncing
volatile uint32_t currentMS;
volatile uint32_t PrevB1Press;
volatile uint32_t PrevB2Press;
volatile uint32_t PrevB3Press;
volatile uint32_t PrevB4Press;

volatile int refreshFlag;

// adjust mode params
volatile int adjMode;
volatile int adjPos;

// RTC time and date
RTC_TimeTypeDef sTime;
RTC_DateTypeDef sDate;

// watch time and date
volatile int hour;
volatile int minute;
volatile int weekDay;
volatile int day;
volatile int month;
volatile int year;

// alarm params
volatile int alarmHour;
volatile int alarmMinute;
volatile int alarmToggle;
volatile int alarmPlay;
volatile int alarmStopped;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM6_Init(void);
static void MX_TIM16_Init(void);
static void MX_RTC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * Get the total amount of milliseconds that have passed from the RTC
 * @return time in milliseconds
 */
uint32_t RTC_GetTotalMS(void) {
    RTC_TimeTypeDef t;
    RTC_DateTypeDef d;
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);

    // calculate total milliseconds
    uint32_t ms = (255 - t.SubSeconds) / 256 * 1000;
    return (uint32_t)t.Hours * 3600000
         + (uint32_t)t.Minutes * 60000
         + (uint32_t)t.Seconds * 1000
    	 + ms;
}

/**
 * Timer interrupt handler
 * @param timer that elapsed
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim){
	if (htim == &htim6) {
		// de-bounce
		if (HAL_GPIO_ReadPin(GPIOA, B3_Pin) == GPIO_PIN_SET && (RTC_GetTotalMS() - PrevB3Press) >= 1000) {
			// toggle adjust mode
			adjMode = adjMode ^ 1;
			adjPos = 0;

			// refresh
			refreshFlag = 1;
			HAL_TIM_Base_Stop_IT(htim);
		}
		timerRunning = 0;
	} else if (htim == &htim16) {
		// play alarm
		HAL_GPIO_TogglePin(GPIOA, BUZZER_Pin);
	}

}

/**
 * RTC wake-up handler
 * @param RTC that woke up
 */
void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc) {
	// refresh
	refreshFlag = 1;
}

/**
 * Get the max day given the current month
 * @return the number of the last day in the current month
 */
int maxDay() {
	// September, April, June, November
	if (month == 9 || month == 4 || month == 6 || month == 11) {
		return 30;
	} else if (month == 2) { // February
		// leap year
		if (year % 4 == 0 && (year % 100 != 0 || (year % 100 == 0 && year % 400 == 0))) {
			return 29;
		}

		return 28;
	} else {
		return 31;
	}
}

/**
 * Hardware interrupt handler
 * @param The GPIO pin that the interrupt occurred on
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	// resume high speed clock
	SystemClock_Config();
	HAL_ResumeTick();

	if (alarmPlay) {
		// stop alarm
		HAL_TIM_Base_Stop_IT(&htim16);
		alarmPlay = 0;
		alarmStopped = 1;
		timerRunning = 0;
	}

	// get total milliseconds that have passed for button de-bouncing
	currentMS = RTC_GetTotalMS();

	if (GPIO_Pin == B4_Pin) {
		// de-bounce
		if (!adjMode && (currentMS - PrevB4Press) >= 1000 && HAL_GPIO_ReadPin(GPIOA, B4_Pin) == GPIO_PIN_SET) {
			PrevB4Press = currentMS;

			// toggle alarm
			alarmToggle ^= 1;

			// refresh
			refreshFlag = 1;
		}
	}

	if (GPIO_Pin == B3_Pin) {
		// de-bounce
		if ((currentMS - PrevB3Press) >= 1000 && HAL_GPIO_ReadPin(GPIOA, B3_Pin) == GPIO_PIN_SET) {
			PrevB3Press = currentMS;

			// start adjust mode timer
			timerRunning = 1;
			HAL_TIM_Base_Stop_IT(&htim6);
			__HAL_TIM_SET_COUNTER(&htim6, 0);
			HAL_TIM_Base_Start_IT(&htim6);
		}
	}

	if (GPIO_Pin == B2_Pin) {
		// de-bounce
		if ((currentMS - PrevB2Press) >= 10000 && HAL_GPIO_ReadPin(GPIOB, B2_Pin) == GPIO_PIN_SET && adjMode) {
			PrevB2Press = currentMS;

			// adjust watch
			switch (adjPos) {
			case 0:
				hour = hour + 1;

				if (hour >= 24) {
					hour = 0;
				}

				sTime.Hours = hour;
				HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
				break;
			case 1:
				minute = minute + 1;

				if (minute >= 60) {
					minute = 0;
				}

				sTime.Minutes = minute;
				HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
				break;
			case 2:
				weekDay++;

				if (weekDay > 6) {
					weekDay = 0;
				}

				sDate.WeekDay = weekDay;
				HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
				break;
			case 3:
				month++;

				if (month > 12) {
					month = 1;
				}

				sDate.Month = month;
				HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
				break;
			case 4:
				day++;

				if (day > maxDay()) {
					day = 1;
				}

				sDate.Date = day;
				HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
				break;
			case 5:
				year++;

				if (year > 2099) {
					year = 2000;
				}

				sDate.Year = year;
				HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
				break;
			case 6:
				alarmToggle ^= 1;
				break;
			case 7:
				alarmHour++;

				if (alarmHour > 24) {
					alarmHour = 1;
				}
				break;
			case 8:
				alarmMinute++;

				if (alarmMinute >= 60) {
					alarmMinute = 0;
				}
				break;
			default:
			}

			refreshFlag = 1;
		}
	}

	if (GPIO_Pin == B1_Pin) {
		// de-bounce
		if ((currentMS - PrevB1Press) >= 1000 && HAL_GPIO_ReadPin(GPIOB, B1_Pin) == GPIO_PIN_SET) {
			PrevB1Press = currentMS;

			if (adjMode) {
				// change adjust mode position
				adjPos++;
				refreshFlag = 1;
				if (adjPos > 9) {
					adjPos = 0;
				}
			}
		}
	}
}

/**
 * Get string from numeric day of the week
 * @param day	the numeric representation of the day of the week
 * @return the string representation of the day of the week
 */
const char* dayString(uint8_t day) {
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

/**
 * Refresh the display
 */
void refresh(void) {
	// display adjust mode position
	if (adjMode) {
	  switch(adjPos) {
	  case 0:
		  Paint_ClearWindows(smallWidth * 3, EPD_1IN54_V2_HEIGHT - smallHeight, smallWidth * 5, EPD_1IN54_V2_HEIGHT, WHITE);
		  Paint_DrawNum(smallWidth * 3, EPD_1IN54_V2_HEIGHT - smallHeight, alarmMinute / 10, &smallFont, WHITE, BLACK);
		  Paint_DrawNum(smallWidth * 4, EPD_1IN54_V2_HEIGHT - smallHeight, alarmMinute % 10, &smallFont, WHITE, BLACK);

		  Paint_DrawNum(100 - timeWidth * 2.5, 100 - timeHeight / 2, hour / 10, &timeFont, BLACK, WHITE);
		  Paint_DrawNum(100 - timeWidth * 1.5, 100 - timeHeight / 2, hour % 10, &timeFont, BLACK, WHITE);
		  break;
	  case 1:
		  Paint_ClearWindows(100 - timeWidth * 2.5, 100 - timeHeight / 2, 100 - timeWidth * 0.5, 100 + timeHeight / 2, WHITE);
		  Paint_DrawNum(100 - timeWidth * 2.5, 100 - timeHeight / 2, hour / 10, &timeFont, WHITE, BLACK);
		  Paint_DrawNum(100 - timeWidth * 1.5, 100 - timeHeight / 2, hour % 10, &timeFont, WHITE, BLACK);

		  Paint_DrawNum(100 + timeWidth / 2, 100 - timeHeight / 2, minute / 10, &timeFont, BLACK, WHITE);
		  Paint_DrawNum(100 + timeWidth * 1.5, 100 - timeHeight / 2, minute % 10, &timeFont, BLACK, WHITE);
		  break;
	  case 2:
		  Paint_ClearWindows(100 + timeWidth / 2, 100 - timeHeight / 2, 100 + timeWidth * 2.5, 100 + timeHeight / 2, WHITE);
		  Paint_DrawNum(100 + timeWidth / 2, 100 - timeHeight / 2, minute / 10, &timeFont, WHITE, BLACK);
		  Paint_DrawNum(100 + timeWidth * 1.5, 100 - timeHeight / 2, minute % 10, &timeFont, WHITE, BLACK);

		  Paint_DrawString_EN(0, 0, dayString(weekDay), &smallFont, WHITE, BLACK);
		  break;
	  case 3:
		  Paint_ClearWindows(0, 0, smallWidth*3, smallHeight, WHITE);
		  Paint_DrawString_EN(0, 0, dayString(weekDay), &smallFont, BLACK, WHITE);

		  Paint_DrawNum(0, smallHeight, month / 10, &smallFont, BLACK, WHITE);
		  Paint_DrawNum(smallWidth, smallHeight, month % 10, &smallFont, BLACK, WHITE);
		  break;
	  case 4:
		  Paint_ClearWindows(0, smallHeight, smallWidth*2, smallHeight*2, WHITE);
		  Paint_DrawNum(0, smallHeight, month / 10, &smallFont, WHITE, BLACK);
		  Paint_DrawNum(smallWidth, smallHeight, month % 10, &smallFont, WHITE, BLACK);

		  Paint_DrawNum(smallWidth * 3, smallHeight, day / 10, &smallFont, BLACK, WHITE);
		  Paint_DrawNum(smallWidth * 4, smallHeight, day % 10, &smallFont, BLACK, WHITE);
		  break;
	  case 5:
		  Paint_ClearWindows(smallWidth*3, smallHeight, smallWidth*5, smallHeight*2, WHITE);
		  Paint_DrawNum(smallWidth * 3, smallHeight, day / 10, &smallFont, WHITE, BLACK);
		  Paint_DrawNum(smallWidth * 4, smallHeight, day % 10, &smallFont, WHITE, BLACK);

		  Paint_DrawNum(smallWidth * 6, smallHeight, year, &smallFont, BLACK, WHITE);
		  break;
	  case 6:
		  Paint_ClearWindows(smallWidth*6, smallHeight, smallWidth*10, smallHeight*2, WHITE);
		  Paint_DrawNum(smallWidth * 6, smallHeight, year, &smallFont, WHITE, BLACK);

		  Paint_DrawString_EN(smallWidth * 6, EPD_1IN54_V2_HEIGHT - smallHeight * 2, (alarmToggle == 0) ? "OFF" : "ON ", &smallFont, WHITE, BLACK);
		  break;
	  case 7:
		  Paint_ClearWindows(smallWidth*6, EPD_1IN54_V2_HEIGHT - smallHeight * 2, smallWidth*9, EPD_1IN54_V2_HEIGHT - smallHeight, WHITE);
		  Paint_DrawString_EN(smallWidth * 6, EPD_1IN54_V2_HEIGHT - smallHeight * 2, (alarmToggle == 0) ? "OFF" : "ON ", &smallFont, BLACK, WHITE);

		  Paint_DrawNum(0, EPD_1IN54_V2_HEIGHT - smallHeight, alarmHour / 10, &smallFont, BLACK, WHITE);
		  Paint_DrawNum(smallWidth, EPD_1IN54_V2_HEIGHT - smallHeight, alarmHour % 10, &smallFont, BLACK, WHITE);
		  break;
	  case 8:
		  Paint_ClearWindows(0, EPD_1IN54_V2_HEIGHT - smallHeight, smallWidth*2, EPD_1IN54_V2_HEIGHT, WHITE);
		  Paint_DrawNum(0, EPD_1IN54_V2_HEIGHT - smallHeight, alarmHour / 10, &smallFont, WHITE, BLACK);
		  Paint_DrawNum(smallWidth, EPD_1IN54_V2_HEIGHT - smallHeight, alarmHour % 10, &smallFont, WHITE, BLACK);

		  Paint_DrawNum(smallWidth * 3, EPD_1IN54_V2_HEIGHT - smallHeight, alarmMinute / 10, &smallFont, BLACK, WHITE);
		  Paint_DrawNum(smallWidth * 4, EPD_1IN54_V2_HEIGHT - smallHeight, alarmMinute % 10, &smallFont, BLACK, WHITE);
		  break;
	  default:
	  }
  } else {
	  // get and set time
	  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
	  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

	  hour = sTime.Hours;
	  minute = sTime.Minutes;
	  weekDay = sDate.WeekDay;
	  day = sDate.Date;
	  month = sDate.Month;
	  year = sDate.Year;

	  if (day > maxDay()) {
		  day = 1;
		  month++;

		  if (month > 12) {
			  month = 1;
			  year++;
		  }
	  }

	  if (alarmToggle && !alarmStopped && !alarmPlay
		&& hour == alarmHour && minute == alarmMinute) {
		  // play alarm
		  HAL_TIM_Base_Start_IT(&htim16);
		  alarmPlay = 1;
		  timerRunning = 1;
	  }

	  if (alarmStopped && hour == alarmHour && minute == alarmMinute + 1) {
		  // stop alarm
		  alarmStopped = 0;
	  }

	  // draw the watch face
	  Paint_ClearWindows(100 - timeWidth * 2.5, 100 - timeHeight / 2, 100 + timeWidth*2.5, 100 + timeHeight / 2, WHITE);
	  Paint_DrawNum(100 - timeWidth * 2.5, 100 - timeHeight / 2, hour / 10, &timeFont, WHITE, BLACK);
	  Paint_DrawNum(100 - timeWidth * 1.5, 100 - timeHeight / 2, hour % 10, &timeFont, WHITE, BLACK);
	  Paint_DrawChar(100 - timeWidth / 2, 100 - timeHeight / 2, ':', &timeFont, BLACK, WHITE);
	  Paint_DrawNum(100 + timeWidth / 2, 100 - timeHeight / 2, minute / 10, &timeFont, WHITE, BLACK);
	  Paint_DrawNum(100 + timeWidth * 1.5, 100 - timeHeight / 2, minute % 10, &timeFont, WHITE, BLACK);

	  Paint_ClearWindows(0, 0, smallWidth*10, smallHeight*2, WHITE);
	  Paint_DrawString_EN(0, 0, dayString(weekDay), &smallFont, BLACK, WHITE);
	  Paint_DrawNum(0, smallHeight, month / 10, &smallFont, WHITE, BLACK);
	  Paint_DrawNum(smallWidth, smallHeight, month % 10, &smallFont, WHITE, BLACK);
	  Paint_DrawChar(smallWidth * 2, smallHeight, '-', &smallFont, BLACK, WHITE);
	  Paint_DrawNum(smallWidth * 3, smallHeight, day / 10, &smallFont, WHITE, BLACK);
	  Paint_DrawNum(smallWidth * 4, smallHeight, day % 10, &smallFont, WHITE, BLACK);
	  Paint_DrawChar(smallWidth * 5, smallHeight, '-', &smallFont, BLACK, WHITE);
	  Paint_DrawNum(smallWidth * 6, smallHeight, year, &smallFont, WHITE, BLACK);

	  Paint_ClearWindows(0, EPD_1IN54_V2_HEIGHT - smallHeight * 2, smallWidth*9, EPD_1IN54_V2_HEIGHT, WHITE);
	  Paint_DrawString_EN(0, EPD_1IN54_V2_HEIGHT - smallHeight * 2, "ALARM", &smallFont, BLACK, WHITE);
	  Paint_DrawString_EN(smallWidth * 6, EPD_1IN54_V2_HEIGHT - smallHeight * 2, (alarmToggle == 0) ? "OFF" : "ON ", &smallFont, BLACK, WHITE);
	  Paint_DrawNum(0, EPD_1IN54_V2_HEIGHT - smallHeight, alarmHour / 10, &smallFont, WHITE, BLACK);
	  Paint_DrawNum(smallWidth, EPD_1IN54_V2_HEIGHT - smallHeight, alarmHour % 10, &smallFont, WHITE, BLACK);
	  Paint_DrawChar(smallWidth * 2, EPD_1IN54_V2_HEIGHT - smallHeight, ':', &smallFont, BLACK, WHITE);
	  Paint_DrawNum(smallWidth * 3, EPD_1IN54_V2_HEIGHT - smallHeight, alarmMinute / 10, &smallFont, WHITE, BLACK);
	  Paint_DrawNum(smallWidth * 4, EPD_1IN54_V2_HEIGHT - smallHeight, alarmMinute % 10, &smallFont, WHITE, BLACK);
  }

  EPD_1IN54_V2_DisplayPart(BlackImage);
  refreshFlag = 0;
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
  MX_TIM16_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */

  // init EPD
  DEV_Module_Init();
  EPD_1IN54_V2_Init();
  EPD_1IN54_V2_Clear();
  DEV_Delay_ms(500);

  // font constants
  timeFont = Consolas28;
  timeWidth = timeFont.Width;
  timeHeight = timeFont.Height;

  smallFont = Font24;
  smallWidth = smallFont.Width;
  smallHeight = smallFont.Height;

  // clear display
  Paint_NewImage(BlackImage, EPD_1IN54_V2_WIDTH, EPD_1IN54_V2_HEIGHT, 270, WHITE);

  EPD_1IN54_V2_Init_Partial();
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);

  // get and set time
  HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
  HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

  hour = sTime.Hours;
  minute = sTime.Minutes;
  weekDay = sDate.WeekDay;
  day = sDate.Date;
  month = sDate.Month;
  year = sDate.Year;

  alarmHour = 8;
  alarmMinute = 0;
  alarmToggle = 1;

  // draw the watch face
  Paint_DrawNum(100 - timeWidth * 2.5, 100 - timeHeight / 2, hour / 10, &timeFont, WHITE, BLACK);
  Paint_DrawNum(100 - timeWidth * 1.5, 100 - timeHeight / 2, hour % 10, &timeFont, WHITE, BLACK);
  Paint_DrawChar(100 - timeWidth / 2, 100 - timeHeight / 2, ':', &timeFont, BLACK, WHITE);
  Paint_DrawNum(100 + timeWidth / 2, 100 - timeHeight / 2, minute / 10, &timeFont, WHITE, BLACK);
  Paint_DrawNum(100 + timeWidth * 1.5, 100 - timeHeight / 2, minute % 10, &timeFont, WHITE, BLACK);

  Paint_DrawString_EN(0, 0, dayString(weekDay), &smallFont, BLACK, WHITE);
  Paint_DrawNum(0, smallHeight, month / 10, &smallFont, WHITE, BLACK);
  Paint_DrawNum(smallWidth, smallHeight, month % 10, &smallFont, WHITE, BLACK);
  Paint_DrawChar(smallWidth * 2, smallHeight, '-', &smallFont, BLACK, WHITE);
  Paint_DrawNum(smallWidth * 3, smallHeight, day / 10, &smallFont, WHITE, BLACK);
  Paint_DrawNum(smallWidth * 4, smallHeight, day % 10, &smallFont, WHITE, BLACK);
  Paint_DrawChar(smallWidth * 5, smallHeight, '-', &smallFont, BLACK, WHITE);
  Paint_DrawNum(smallWidth * 6, smallHeight, year, &smallFont, WHITE, BLACK);

  Paint_DrawString_EN(0, EPD_1IN54_V2_HEIGHT - smallHeight * 2, "ALARM", &smallFont, BLACK, WHITE);
  Paint_DrawString_EN(smallWidth * 6, EPD_1IN54_V2_HEIGHT - smallHeight * 2, (alarmToggle == 0) ? "OFF" : "ON ", &smallFont, BLACK, WHITE);
  Paint_DrawNum(0, EPD_1IN54_V2_HEIGHT - smallHeight, alarmHour / 10, &smallFont, WHITE, BLACK);
  Paint_DrawNum(smallWidth, EPD_1IN54_V2_HEIGHT - smallHeight, alarmHour % 10, &smallFont, WHITE, BLACK);
  Paint_DrawChar(smallWidth * 2, EPD_1IN54_V2_HEIGHT - smallHeight, ':', &smallFont, BLACK, WHITE);
  Paint_DrawNum(smallWidth * 3, EPD_1IN54_V2_HEIGHT - smallHeight, alarmMinute / 10, &smallFont, WHITE, BLACK);
  Paint_DrawNum(smallWidth * 4, EPD_1IN54_V2_HEIGHT - smallHeight, alarmMinute % 10, &smallFont, WHITE, BLACK);

  EPD_1IN54_V2_DisplayPart(BlackImage);

  /* USER CODE END 2 */

  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if (refreshFlag) {
		  // update display
		  refresh();
	  }

	  // check if a timer is running or user is adjusting time
	  if (!timerRunning && !adjMode) {
		  if (refreshFlag) {
			  // update display
			  refresh();
		  }

		  // enter STOP 2 mode to save power
		  HAL_SuspendTick();

		  HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 59, RTC_WAKEUPCLOCK_CK_SPRE_16BITS);
		  HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);
		  HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
		  SystemClock_Config();
		  HAL_ResumeTick();
	  }

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
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

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

  /*Configure GPIO pins : B3_Pin B4_Pin */
  GPIO_InitStruct.Pin = B3_Pin|B4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

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
