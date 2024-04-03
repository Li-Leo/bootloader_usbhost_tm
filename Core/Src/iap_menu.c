/**
  ******************************************************************************
  * @file    USB_Host/FWupgrade_Standalone/Src/iap_menu.c
  * @author  MCD Application Team
  * @brief   COMMAND IAP Execute Application
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------ */
#include "main.h"
#include "fatfs.h"
#include "usb_host.h"
#include "flash_if.h"
#include "command.h"

/* Private typedef ----------------------------------------------------------- */
/* Private define ------------------------------------------------------------ */
/* State Machine for the DEMO State */
#define INIT_STATE    ((uint8_t)0x00)
#define IAP_STATE     ((uint8_t)0x01)

/* Private macro ------------------------------------------------------------- */
/* Private variables --------------------------------------------------------- */
__IO uint32_t UploadCondition = 0x00;
static uint8_t g_state = INIT_STATE;

extern ApplicationTypeDef Appli_state;
extern USBH_HandleTypeDef hUsbHostFS;

/* Private function prototypes ----------------------------------------------- */
static void IAP_UploadTimeout(void);
static void USBH_USR_BufferSizeControl(void);
void FatFs_Fail_Handler(void);

/* Private functions --------------------------------------------------------- */

/**
  * @brief  Demo application for IAP through USB mass storage.
  * @param  None
  * @retval None
  */
void FW_UPGRADE_Process(void)
{
  switch (g_state)
  {
  case INIT_STATE:

    if (f_mount(&USBHFatFS, "", 0) != FR_OK)
    {
      /* FatFs initialization fails */
      /* Toggle LED3 and LED4 in infinite loop */
      FatFs_Fail_Handler();
    }
    printf("mount ok\n");

    /* Go to IAP menu */
    g_state = IAP_STATE;
    break;

  case IAP_STATE:
    while (USBH_MSC_IsReady(&hUsbHostFS)) {
      /* Control BUFFER_SIZE value */
      USBH_USR_BufferSizeControl();

      FLASH_If_FlashUnlock();

      /* Writes Flash memory */
      COMMAND_Download();

      // IAP_UploadTimeout();

      /* Check if USER Button is already pressed */
      if ((UploadCondition == 0x01))
      {
        /* Reads all flash memory */
        COMMAND_Upload();
        // printf("upload done!\n");
      }
      else
      {
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
        // printf("not upload done!\n");
      }

      /* Waiting USER Button Released */
      while (HAL_GPIO_ReadPin(LEFT_SW_GPIO_Port, LEFT_SW_Pin) == GPIO_PIN_RESET && (Appli_state == APPLICATION_READY))
      {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        HAL_Delay(200);
      }

      if (Appli_state == APPLICATION_READY)
      {
        /* Jump to user application code located in the internal Flash memory */
        COMMAND_Jump();
      }
    }
    break;

  default:
    break;
  }

  if (Appli_state == APPLICATION_DISCONNECT)
  {
    /* Toggle LED3: USB device disconnected */
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    HAL_Delay(1000);
  }
}

/**
  * @brief  Button state time control.
  * @param  None
  * @retval None
  */
static void IAP_UploadTimeout(void)
{
  /* Check if KEY button is pressed */
  if (HAL_GPIO_ReadPin(LEFT_SW_GPIO_Port, GPIO_PIN_10) == GPIO_PIN_RESET)
  {
    /* To execute the UPLOAD command the KEY button should be kept pressed 3s
     * just after a board reset, at firmware startup */
    HAL_Delay(5000);

    if (HAL_GPIO_ReadPin(LEFT_SW_GPIO_Port, GPIO_PIN_10) == GPIO_PIN_RESET)
    {
      /* UPLOAD command will be executed immediately after completed execution
       * of the DOWNLOAD command */

      UploadCondition = 0x01;

      /* Turn LED3 and LED4 on : Upload condition Verified */
      // BSP_LED_On(LED3);
      // BSP_LED_On(LED4);

      /* Waiting USER Button Pressed */
      while (HAL_GPIO_ReadPin(LEFT_SW_GPIO_Port, GPIO_PIN_10) == GPIO_PIN_RESET)
      {
      }

    }
    else
    {
      /* Only the DOWNLOAD command is executed */
      UploadCondition = 0x00;
    }
  }
}

/**
  * @brief  Handles the program fail.
  * @param  None
  * @retval None
  */
void Fail_Handler(void)
{
  while (1)
  {
    /* Toggle LED4 */
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    HAL_Delay(100);
  }
}

/**
  * @brief  Handles the Flash Erase fail.
  * @param  None
  * @retval None
  */
void Erase_Fail_Handler(void)
{
  while (1)
  {
    /* Toggle LED4 */
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    HAL_Delay(100);
  }
}

/**
  * @brief  Handles the FatFs fail.
  * @param  None
  * @retval None
  */
void FatFs_Fail_Handler(void)
{
  while (1)
  {
    /* Toggle LED4 */
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    HAL_Delay(100);
  }
}

/**
  * @brief  Controls Buffer size value.
  * @param  None
  * @retval None
  */
static void USBH_USR_BufferSizeControl(void)
{
  /* Control BUFFER_SIZE and limit this value to 32Kbyte maximum */
  if ((BUFFER_SIZE % 4 != 0x00) || (BUFFER_SIZE / 4 > 8192))
  {
    while (1)
    {
      /* Toggle LED4 */
      HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
      HAL_Delay(100);
    }
  }
}
