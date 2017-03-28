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
#include "DiceCore.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
RNG_HandleTypeDef hrng;

#ifndef SILENTDICE
UART_HandleTypeDef huart2;
#endif

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
#ifndef SILENTDICE
#define DEFAULT_FILE_HANDLE_STDOUT (1)
#define DEFAULT_FILE_HANDLE_STDIN (2)
#define DEFAULT_FILE_HANDLE_STDERR (3)
struct __FILE { int handle; };
FILE __stdout = {DEFAULT_FILE_HANDLE_STDOUT};
FILE __stdin = {DEFAULT_FILE_HANDLE_STDIN};
FILE __stderr = {DEFAULT_FILE_HANDLE_STDERR};
#endif
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Error_Handler(void);
static void MX_GPIO_Init(void);
#ifndef SILENTDICE
static void MX_USART2_UART_Init(void);
#endif
static void MX_RNG_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
#ifndef SILENTDICE
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
    return -1;
}

int fgetc(FILE *f)
{
  if(f->handle == DEFAULT_FILE_HANDLE_STDIN)
  {
//      while((!CDC_RX_Data) && (CDC_DTR));
//      if(CDC_DTR)
//      {
//          int ch = *CDC_RX_Data;
//          CDC_RX_Data = NULL;
//          return ch;
//      }
  }
  return -1;
}
#endif

/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */
    DICE_STATUS retVal = DICE_SUCCESS;
  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
#ifndef SILENTDICE
  MX_USART2_UART_Init();
#endif
//  MX_USB_DEVICE_Init();
  MX_RNG_Init();

  /* USER CODE BEGIN 2 */
    EPRINTF("\r\n\r\n++++++++++++++++\r\n");
    EPRINTF("--> miniDICE <--\r\n");
    EPRINTF("++++++++++++++++\r\n");
    if(DiceLockDown() != DICE_SUCCESS) Error_Handler();
    if(((!DICEFUSEAREA->s.deviceInfo.properties.noClear) && (HAL_GPIO_ReadPin(FWBut_GPIO_Port, FWBut_Pin) != GPIO_PIN_RESET)) ||
       ((retVal = DiceInitialize()) == DICE_LOAD_MODULE_FAILED) ||
        (HAL_GPIO_ReadPin(FWBut_GPIO_Port, FWBut_Pin) != GPIO_PIN_RESET))
    {
        MX_USB_DEVICE_Init();
        
        // The only way out of here is a reboot
        EPRINTF("INFO: Reboot to exit DFU mode.\r\n");
        for(;;)
        {
            DiceBlink(DICEBLINKDFU);
        }
    }
    if(retVal != DICE_SUCCESS)
    {
        Error_Handler();
    }
    DicePrintInfoHex("DiceData", DICEDATAPTR, sizeof(DiceData_t));

    // The point of no return: Launch the application
    EPRINTF("INFO: Preparing payload APP launch...\r\n\r\n");
    // Release the clock source
    HAL_RCC_DeInit();
    // Release the HAL
    HAL_DeInit();
    // Redirect the vector table to the application vector table
    SCB->VTOR = DICEAPPLICATIONAREASTART;
    // Wipe all accessible RAM to make sure we didn't leave anything behind
    for(uint32_t n = 0; n < DICEWIPERAMSIZE; n++) ((uint8_t*)DICEWIPERAMSTART)[n] = 0x00;
    // Set the stackpointer to where the application expects it
    __set_MSP(*((uint32_t*)DICEAPPLICATIONAREASTART));
    // Jump into the abyss
    ((void (*)(void)) *((uint32_t*)(DICEAPPLICATIONAREASTART + sizeof(uint32_t))))();
    // We will never return from this call!!

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
    EPRINTF("ERROR: How the hell did we get here?!?\r\n");
    HAL_Delay(1000);

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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSI48;
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

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_USB;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
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

/* USART2 init function */
#ifndef SILENTDICE
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
  if (HAL_HalfDuplex_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }

}
#endif

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
      DiceBlink(DICEBLINKERROR);
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
