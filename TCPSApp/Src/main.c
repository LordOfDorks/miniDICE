/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  *
  * Copyright (c) 2017 STMicroelectronics International N.V. 
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32l0xx_hal.h"
#include "usb_device.h"

/* USER CODE BEGIN Includes */
#include <time.h>
#include "Socket.h"
#include "NTPClient.h"

/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
CRYP_HandleTypeDef hcryp;

RNG_HandleTypeDef hrng;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart4;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
FILE __stdout = {DEFAULT_FILE_HANDLE_STDOUT};
FILE __stdin = {DEFAULT_FILE_HANDLE_STDIN};
FILE __stderr = {DEFAULT_FILE_HANDLE_STDERR};
FILE __net = {DEFAULT_FILE_HANDLE_NET};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Error_Handler(void);
static void MX_GPIO_Init(void);
static void MX_RNG_Init(void);
static void MX_RTC_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_AES_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART4_UART_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/
int fputc(int ch, FILE *f)
{
    if(f->handle == DEFAULT_FILE_HANDLE_STDOUT)
    {
//        while(CDC_Transmit_FS((uint8_t*)&ch, sizeof(uint8_t)) == USBD_BUSY);
//        return ch;
    }
    else if(f->handle == DEFAULT_FILE_HANDLE_STDERR)
    {
        while(HAL_UART_Transmit(&huart2, (uint8_t*)&ch, sizeof(uint8_t), HAL_MAX_DELAY) == HAL_BUSY);
        return ch;
    }
    else if(f->handle == DEFAULT_FILE_HANDLE_NET)
    {
        while(HAL_UART_Transmit(&huart4, (uint8_t*)&ch, sizeof(uint8_t), HAL_MAX_DELAY) == HAL_BUSY);
        return ch;
    }
    return -1;
}

uint8_t rxBuf[16] = {0};
uint32_t rxRd = 0;
uint32_t rxWr = 0;
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart == &huart4)
    {
        rxWr = (rxWr + 1) % sizeof(rxBuf);
        if(rxRd != (rxWr + 1) % sizeof(rxBuf))
        {
            HAL_UART_Receive_IT(&huart4, &rxBuf[rxWr], 1);
        }
    }
}

int fgetc(FILE *f)
{
    int ch = -1;
  if(f->handle == DEFAULT_FILE_HANDLE_STDIN)
  {
//      while((!CDC_RX_Data) && (CDC_DTR));
//      if(CDC_DTR)
//      {
//          ch = *CDC_RX_Data;
//          CDC_RX_Data = NULL;
//          return ch;
//      }
  }
  else if(f->handle == DEFAULT_FILE_HANDLE_NET)
  {
      if(rxRd != (rxWr + 1) % sizeof(rxBuf))
      {
          __HAL_UART_ENABLE_IT(&huart4, UART_IT_RXNE);
          HAL_UART_Receive_IT(&huart4, &rxBuf[rxWr], 1);
      }
      while(rxRd == rxWr) HAL_Delay(1);
      ch = rxBuf[rxRd];
      rxRd = (rxRd + 1) % sizeof(rxBuf);
      HAL_UART_Receive_IT(&huart4, &rxBuf[rxWr], 1);
  }
  return ch;
}

HAL_StatusTypeDef SetTime(time_t t)
{
    HAL_StatusTypeDef retVal = HAL_OK;
    RTC_DateTypeDef date = { 0 };
    RTC_TimeTypeDef time = { 0 };
    struct tm* timeInfo = NULL;

    timeInfo = localtime((time_t*)&t);
    time.Seconds = timeInfo->tm_sec;
    time.Minutes = timeInfo->tm_min;
    time.Hours = timeInfo->tm_hour;
    date.WeekDay = timeInfo->tm_wday;
    date.Date = timeInfo->tm_mday + 1; // mday 0 is the first
    date.Month = timeInfo->tm_mon + 1; // January = 0
    date.Year = timeInfo->tm_year - 100; // Base year is 1900

    if(((retVal = HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN)) != HAL_OK) ||
       ((retVal = HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN)) != HAL_OK))
    {
        goto Cleanup;
    }

Cleanup:
    return retVal;
}

HAL_StatusTypeDef GetTime(time_t* t)
{
    HAL_StatusTypeDef retVal = HAL_OK;
    RTC_DateTypeDef date = { 0 };
    RTC_TimeTypeDef time = { 0 };
    struct tm timeInfo = { 0 };
    
    if(((retVal = HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN)) != HAL_OK) ||
       ((retVal = HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN)) != HAL_OK))
    {
        *t = 0;
        goto Cleanup;
    }

    timeInfo.tm_sec = time.Seconds;
    timeInfo.tm_min = time.Minutes;
    timeInfo.tm_hour = time.Hours;
    timeInfo.tm_wday = date.WeekDay;
    timeInfo.tm_mday = date.Date - 1; // mday 0 is the first
    timeInfo.tm_mon = date.Month - 1; // January = 0
    timeInfo.tm_year = date.Year + 100; // Base year is 1900
    *t = mktime(&timeInfo);

Cleanup:
    return retVal;
}

time_t time(time_t* tOut)
{
    time_t t = 0;
    GetTime(&t);
    if(tOut) *tOut = t;
    return t;
}

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_RNG_Init();
  MX_RTC_Init();
  MX_USART2_UART_Init();
  MX_AES_Init();
  MX_SPI1_Init();
  MX_USART4_UART_Init();
//  MX_USB_DEVICE_Init();

  /* USER CODE BEGIN 2 */
  uint32_t ipAddr = 0;
  uint8_t macAddr[6] = {0};
  InitializeESP8266(&ipAddr, macAddr, HAL_MAX_DELAY);
  fprintf(&__stderr, "\r\n%d.%d.%d.%d (%02x:%02x:%02x:%02x:%02x:%02x)\r\n", (ipAddr >> 24), (0xff & (ipAddr >> 16)), (0xff & (ipAddr >> 8)), (0xff & ipAddr), macAddr[0],  macAddr[1], macAddr[2],  macAddr[3], macAddr[4],  macAddr[5]);
  
  // Set the RTC to UTC
  time_t ntpTime = {0};
  if(NtpGetTime("pool.ntp.org", 4000, &ntpTime) == HAL_OK)
  {
      time_t now = time(NULL);
      SetTime(ntpTime);
      while(now >= time(NULL)); // The RTC needs a couple cycles to reflect the new time
  }
  else
  {
      printf("NtpGetTime() failed.\r\n");
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  time_t last = 0;
  while (1)
  {
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
      time_t now = time(NULL);
      if(last < now)
      {
          time_t local = now - (60 * 60 * 7);
          char* timeStr = asctime(localtime(&local));
          timeStr[strlen(timeStr) - 1] = 0;
          fprintf(&__stderr, "\r%s PDST (0x%08x)", timeStr, now);
          last = now;
      }

  }
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit;

    /**Configure the main internal regulator output voltage 
    */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE
                              |RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_4;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

    /**Initializes the CPU, AHB and APB busses clocks 
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

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_RTC
                              |RCC_PERIPHCLK_USB;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure LSE Drive Capability 
    */
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* AES init function */
static void MX_AES_Init(void)
{

   uint8_t pKey[16] ;

  hcryp.Instance = AES;
  hcryp.Init.DataType = CRYP_DATATYPE_32B;
  pKey[0] = 0x00;
  pKey[1] = 0x00;
  pKey[2] = 0x00;
  pKey[3] = 0x00;
  pKey[4] = 0x00;
  pKey[5] = 0x00;
  pKey[6] = 0x00;
  pKey[7] = 0x00;
  pKey[8] = 0x00;
  pKey[9] = 0x00;
  pKey[10] = 0x00;
  pKey[11] = 0x00;
  pKey[12] = 0x00;
  pKey[13] = 0x00;
  pKey[14] = 0x00;
  pKey[15] = 0x00;
  hcryp.Init.pKey = &pKey[0];
  if (HAL_CRYP_Init(&hcryp) != HAL_OK)
  {
    Error_Handler();
  }

}

/* RNG init function */
static void MX_RNG_Init(void)
{

  hrng.Instance = RNG;
  if (HAL_RNG_Init(&hrng) != HAL_OK)
  {
    Error_Handler();
  }

}

/* RTC init function */
static void MX_RTC_Init(void)
{

  RTC_TimeTypeDef sTime;
  RTC_DateTypeDef sDate;

    /**Initialize RTC Only 
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

    /**Initialize RTC and set the Time and Date 
    */
  if(HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) != 0x32F2){
  sTime.Hours = 0;
  sTime.Minutes = 0;
  sTime.Seconds = 0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }

  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 1;
  sDate.Year = 0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }

    HAL_RTCEx_BKUPWrite(&hrtc,RTC_BKP_DR0,0x32F2);
  }

}

/* SPI1 init function */
static void MX_SPI1_Init(void)
{

  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_HARD_OUTPUT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }

}

/* USART2 init function */
static void MX_USART2_UART_Init(void)
{

  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }

}

/* USART4 init function */
static void MX_USART4_UART_Init(void)
{

  huart4.Instance = USART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LD3_Pin */
  GPIO_InitStruct.Pin = LD3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD3_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : FWBut_Pin */
  GPIO_InitStruct.Pin = FWBut_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(FWBut_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM2 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
/* USER CODE BEGIN Callback 0 */

/* USER CODE END Callback 0 */
  if (htim->Instance == TIM2) {
    HAL_IncTick();
  }
/* USER CODE BEGIN Callback 1 */

/* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler */
  /* User can add his own implementation to report the HAL error return state */
  while(1) 
  {
  }
  /* USER CODE END Error_Handler */ 
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
