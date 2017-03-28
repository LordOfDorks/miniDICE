/**
  ******************************************************************************
  * @file           : usbd_dfu_if.c
  * @brief          :
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
#include "usbd_dfu_if.h"
/* USER CODE BEGIN INCLUDE */
#include "DiceCore.h"
/* USER CODE END INCLUDE */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @{
  */
/** @defgroup USBD_DFU 
  * @brief usbd core module
  * @{
  */ 

/** @defgroup USBD_DFU_Private_TypesDefinitions
  * @{
  */ 
/* USER CODE BEGIN PRIVATE_TYPES */
/* USER CODE END PRIVATE_TYPES */ 
/**
  * @}
  */ 

/** @defgroup USBD_DFU_Private_Defines
  * @{
  */ 
#define FLASH_DESC_STR      "@Dice /0x08005400/43*001Kg/0x08010000/64*001Kg/0x08020000/64*001Kg/0x08080000/1*256Bf/0x08080100/79*256Bg"
/* USER CODE BEGIN PRIVATE_DEFINES */
#define FLASH_ERASE_TIME    (uint16_t)50
#define FLASH_PROGRAM_TIME  (uint16_t)50
/* USER CODE END PRIVATE_DEFINES */
  
/**
  * @}
  */ 

/** @defgroup USBD_DFU_Private_Macros
  * @{
  */ 
/* USER CODE BEGIN PRIVATE_MACRO */
/* USER CODE END PRIVATE_MACRO */

/**
  * @}
  */ 

/** @defgroup USBD_AUDIO_IF_Private_Variables
  * @{
  */
/* USER CODE BEGIN PRIVATE_VARIABLES */
/* USER CODE END PRIVATE_VARIABLES */

/**
  * @}
  */ 
  
/** @defgroup USBD_DFU_IF_Exported_Variables
  * @{
  */ 
  extern USBD_HandleTypeDef hUsbDeviceFS;
/* USER CODE BEGIN EXPORTED_VARIABLES */
/* USER CODE END EXPORTED_VARIABLES */

/**
  * @}
  */ 
  
/** @defgroup USBD_DFU_Private_FunctionPrototypes
  * @{
  */
static uint16_t MEM_If_Init_FS(void);
static uint16_t MEM_If_Erase_FS (uint32_t Add);
static uint16_t MEM_If_Write_FS (uint8_t *src, uint8_t *dest, uint32_t Len);
static uint8_t *MEM_If_Read_FS  (uint8_t *src, uint8_t *dest, uint32_t Len);
static uint16_t MEM_If_DeInit_FS(void);
static uint16_t MEM_If_GetStatus_FS (uint32_t Add, uint8_t Cmd, uint8_t *buffer);

/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */

/* USER CODE END PRIVATE_FUNCTIONS_DECLARATION */

/**
  * @}
  */ 
  
#if defined ( __ICCARM__ ) /*!< IAR Compiler */
  #pragma data_alignment=4   
#endif
__ALIGN_BEGIN USBD_DFU_MediaTypeDef USBD_DFU_fops_FS __ALIGN_END =
{
   (uint8_t*)FLASH_DESC_STR,
    MEM_If_Init_FS,
    MEM_If_DeInit_FS,
    MEM_If_Erase_FS,
    MEM_If_Write_FS,
    MEM_If_Read_FS,
    MEM_If_GetStatus_FS,   
};

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  MEM_If_Init_FS
  *         Memory initialization routine.
  * @param  None
  * @retval 0 if operation is successful, MAL_FAIL else.
  */
uint16_t MEM_If_Init_FS(void)
{ 
  /* USER CODE BEGIN 0 */ 
    uint16_t result = USBD_FAIL;
    if(HAL_FLASH_Unlock() == HAL_OK)
    {
    result = USBD_OK;
    }
//    EPRINTF("INFO: MEM_If_Init_FS() = %s\r\n", (result==USBD_OK) ? "USBD_OK" : "USBD_FAIL");
    return result;
  /* USER CODE END 0 */ 
}

/**
  * @brief  MEM_If_DeInit_FS
  *         De-Initializes Memory.
  * @param  None
  * @retval 0 if operation is successful, MAL_FAIL else.
  */
uint16_t MEM_If_DeInit_FS(void)
{ 
  /* USER CODE BEGIN 1 */ 
    uint16_t result = USBD_FAIL;
    if(HAL_FLASH_Lock() == HAL_OK)
    {
    result = USBD_OK;
    }
//    EPRINTF("INFO: MEM_If_DeInit_FS() = %s\r\n", (result==USBD_OK) ? "USBD_OK" : "USBD_FAIL");
    return result;
  /* USER CODE END 1 */ 
}

/**
  * @brief  MEM_If_Erase_FS
  *         Erase sector.
  * @param  Add: Address of sector to be erased.
  * @retval 0 if operation is successful, MAL_FAIL else.
  */
uint16_t MEM_If_Erase_FS(uint32_t Add)
{
  /* USER CODE BEGIN 2 */ 
  uint16_t result = USBD_FAIL;
  HAL_StatusTypeDef halResult = HAL_OK;

  if((Add >= 0x08000000) && (Add < 0x08030000))
  {
      uint32_t PageError = 0;
      FLASH_EraseInitTypeDef erase = {FLASH_TYPEERASE_PAGES, Add, 8};
      for(uint32_t n = 0, halResult = HAL_BUSY; ((n < 10) && ((PageError != 0xffffffff) || (halResult != HAL_OK))); n++)
      {
          halResult = HAL_FLASHEx_Erase(&erase, &PageError);
      }
      if((PageError == 0xffffffff) && (halResult == HAL_OK))
      {
          result = USBD_OK;
          goto Cleanup;
      }
  }
  else if(((!DiceData.s.cert.signData.deviceInfo.properties.noClear) && ((Add >= 0x08080000) && (Add < 0x080800100))) ||
          ((Add >= 0x08080100) && (Add < 0x080850000)))
  {
      for(uint32_t n = 0; n < 256; n += sizeof(uint32_t))
      {
          for(uint32_t n = 0, halResult = HAL_BUSY; ((n < 10) && (halResult != HAL_OK)); n++)
          {
              if((halResult = HAL_FLASHEx_DATAEEPROM_Erase(Add + n)) != HAL_OK)
              {
                  result = USBD_FAIL;
                  goto Cleanup;
              }
          }
      }
      result = USBD_OK;
  }

Cleanup:
//  EPRINTF("INFO: MEM_If_Erase_FS(0x%08x) = %s\r\n", Add, (result==USBD_OK) ? "USBD_OK" : "USBD_FAIL");
  if(result == USBD_OK)
      EPRINTF(".");
  else
      EPRINTF("x");
  return result;
  /* USER CODE END 2 */ 
}

/**
  * @brief  MEM_If_Write_FS
  *         Memory write routine.
  * @param  src: Pointer to the source buffer. Address to be written to.
  * @param  dest: Pointer to the destination buffer.
  * @param  Len: Number of data to be written (in bytes).
  * @retval 0 if operation is successful, MAL_FAIL else.
  */
uint16_t MEM_If_Write_FS(uint8_t *src, uint8_t *dest, uint32_t Len)
{
  /* USER CODE BEGIN 3 */ 
  uint16_t result = USBD_FAIL;
  HAL_StatusTypeDef halResult = HAL_OK;
    
  for(uint32_t n = 0; n < Len; n += sizeof(uint32_t))
  {
      for(uint32_t m = 0, halResult = HAL_BUSY; ((m < 10) && (halResult == HAL_BUSY)); m++)
      {
          if(((uint32_t)dest >= 0x08000000) && ((uint32_t)dest < 0x08030000))
          {
              halResult = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)&dest[n], *((uint32_t*)&src[n]));
          }
          else if(((!DiceData.s.cert.signData.deviceInfo.properties.noClear) && (((uint32_t)dest >= 0x08080000) && ((uint32_t)dest < 0x080800100))) ||
                  (((uint32_t)dest >= 0x08080100) && ((uint32_t)dest < 0x080850000)))
          {
              halResult = HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAM_WORD, (uint32_t)&dest[n], *((uint32_t*)&src[n]));
          }
          if(*((uint32_t*)&src[n]) != *((uint32_t*)&dest[n]))
          {
              goto Cleanup;
          }
      }
  }
  if(halResult == HAL_OK)
  {
      result = USBD_OK;
  }

Cleanup:
//  EPRINTF("INFO: MEM_If_Write_FS(0x%08x, 0x%08x, %d) = %s\r\n", (unsigned int)src, (unsigned int)dest, Len, (result==USBD_OK) ? "USBD_OK" : "USBD_FAIL");
  if(result == USBD_OK)
      EPRINTF("o");
  else
      EPRINTF("x");
  return result;
  /* USER CODE END 3 */ 
}

/**
  * @brief  MEM_If_Read_FS
  *         Memory read routine.
  * @param  src: Pointer to the source buffer. Address to be written to.
  * @param  dest: Pointer to the destination buffer.
  * @param  Len: Number of data to be read (in bytes).
  * @retval Pointer to the physical address where data should be read.
  */
uint8_t *MEM_If_Read_FS (uint8_t *src, uint8_t *dest, uint32_t Len)
{
  /* Return a valid address to avoid HardFault */
  /* USER CODE BEGIN 4 */ 
    if(((uint32_t)src >= 0x08080000) && ((uint32_t)src < 0x080800100))
    {
        memcpy(dest, src, Len);
//        EPRINTF("INFO: MEM_If_Read_FS(0x%08x, 0x%08x, %d) = USBD_OK\r\n", (unsigned int)src, (unsigned int)dest, Len);
        EPRINTF("O");
    }
    else
    {
        memset(dest, 0x00, Len);
    }
    return (uint8_t*)(USBD_OK);
  /* USER CODE END 4 */ 
}

/**
  * @brief  Flash_If_GetStatus_FS
  *         Get status routine.
  * @param  Add: Address to be read from.
  * @param  Cmd: Number of data to be read (in bytes).
  * @param  buffer: used for returning the time necessary for a program or an erase operation
  * @retval 0 if operation is successful
  */
uint16_t MEM_If_GetStatus_FS (uint32_t Add, uint8_t Cmd, uint8_t *buffer)
{
  /* USER CODE BEGIN 5 */ 
  switch (Cmd)
  {
  case DFU_MEDIA_PROGRAM:
    buffer[1] = (uint8_t)FLASH_PROGRAM_TIME;
    buffer[2] = (uint8_t)(FLASH_PROGRAM_TIME << 8);
    buffer[3] = 0;  
    break;
    
  case DFU_MEDIA_ERASE:
  default:
    buffer[1] = (uint8_t)FLASH_ERASE_TIME;
    buffer[2] = (uint8_t)(FLASH_ERASE_TIME << 8);
    buffer[3] = 0;
    break;
  } 
//  EPRINTF("INFO: MEM_If_GetStatus_FS(0x%08x, %d, 0x%08x) = USBD_OK\r\n", Add, Cmd, (unsigned int)buffer);
  return  (USBD_OK);
  /* USER CODE END 5 */  
}

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */
/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
  * @}
  */ 

/**
  * @}
  */  
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

