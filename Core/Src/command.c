/**
  ******************************************************************************
  * @file    USB_Host/FWupgrade_Standalone/Src/command.c
  * @author  MCD Application Team
  * @brief   This file provides all the IAP command functions.
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
#include "flash_if.h"
#include "usb_host.h"
#include "fatfs.h"
#include "stdint.h"

/* Private typedef ----------------------------------------------------------- */
/* Private defines ----------------------------------------------------------- */
#define UPLOAD_FILENAME            "0:UPLOAD.bin"
#define DOWNLOAD_FILENAME          "tm_image.bin"
#define FILENAME                   "TM_IMAGE.BIN"

/* Private macros ------------------------------------------------------------ */
/* Private variables --------------------------------------------------------- */
static uint32_t TmpReadSize = 0x00;
static uint32_t RamAddress = 0x00;
static __IO uint32_t LastPGAddress = APPLICATION_ADDRESS;
static uint8_t RAM_Buf[BUFFER_SIZE] = { 0x00 };

FATFS USBH_fatfs;
FIL up_load_file;                     /* File object for upload operation */
FIL down_load_file;                    /* File object for download operation */

extern ApplicationTypeDef Appli_state;
extern USBH_HandleTypeDef hUsbHostFS;

/* Private function prototypes ----------------------------------------------- */
static void COMMAND_ProgramFlashMemory(void);
void find_bin_file(const char *name);
FRESULT find_file(const TCHAR* path, const TCHAR* ext, TCHAR* foundFile);

/* Private functions --------------------------------------------------------- */

/**
  * @brief  IAP Read all flash memory.
  * @param  None
  * @retval None
  */
void COMMAND_Upload(void)
{
  __IO uint32_t address = APPLICATION_ADDRESS;
  __IO uint32_t counterread = 0x00;
  uint32_t tmpcounter = 0x00, indexoffset = 0x00;
  FlagStatus readoutstatus = SET;
  uint16_t byteswritten;

  /* Get the read out protection status */
  readoutstatus = FLASH_If_ReadOutProtectionStatus();
  if (readoutstatus == RESET)
  {
    /* Remove UPLOAD file if it exists on flash disk */
    f_unlink(UPLOAD_FILENAME);

    /* Init written byte counter */
    indexoffset = (APPLICATION_ADDRESS - USER_FLASH_STARTADDRESS);

    /* Open binary file to write on it */
    if ((Appli_state == APPLICATION_READY) &&
        (f_open(&up_load_file, UPLOAD_FILENAME, FA_CREATE_ALWAYS | FA_WRITE) ==
         FR_OK))
    {

      /* Read flash memory */
      while ((indexoffset < USER_FLASH_SIZE) &&
             (Appli_state == APPLICATION_READY))
      {
        for (counterread = 0; counterread < BUFFER_SIZE; counterread++)
        {
          /* Check the read bytes versus the end of flash */
          if (indexoffset + counterread < USER_FLASH_SIZE)
          {
            tmpcounter = counterread;
            RAM_Buf[tmpcounter] = (*(uint8_t *) (address++));
          }
          /* In this case all flash was read */
          else
          {
            break;
          }
        }

        /* Write buffer to file */
        f_write(&up_load_file, RAM_Buf, BUFFER_SIZE, (void *)&byteswritten);

        /* Number of byte written */
        indexoffset = indexoffset + counterread;
      }


      /* Close file and filesystem */
      f_close(&up_load_file);
      f_mount(0, 0, 0);
    }
  }
  else
  {
    /* Message ROP active: Turn LED3 On and Toggle LED4 in infinite loop */
    Fail_Handler();
  }
}

/**
  * @brief  IAP Write Flash memory.
  * @param  None
  * @retval None
  */
void COMMAND_Download(void)
{
  char file_name[13] = {0};

  find_file("", ".bin", file_name);

  if (strcmp(file_name, FILENAME) == 0) {
    printf("find_file bin %s\n", file_name);
  } else {
    printf("no bin file\n");
    return;
  }
  
  /* Open the binary file to be downloaded */
  if (f_open(&down_load_file, DOWNLOAD_FILENAME, FA_OPEN_EXISTING | FA_READ) == FR_OK)
  {
    if (f_size(&down_load_file) > USER_FLASH_SIZE)
    {
      Fail_Handler();
    }
    else
    {
      /* Erase FLASH sectors to download image */
      if (FLASH_If_EraseSectors(APPLICATION_ADDRESS) != 0x00)
      {
        Erase_Fail_Handler();
      } else {
        printf("erase ok\n");
      }

      /* Program flash memory */
      COMMAND_ProgramFlashMemory();

      /* Close file */
      f_close(&down_load_file);
      printf("pragrammed done\n");
    }
  }
  else
  {
    /* The binary file is not available: Toggle LED4 in infinite loop */
    Fail_Handler();
  }
}

/**
  * @brief  IAP jump to user program.
  * @param  None
  * @retval None
  */
void COMMAND_Jump(void)
{
  /* Software reset */
  NVIC_SystemReset();
}

/**
  * @brief  Programs the internal Flash memory.
  * @param  None
  * @retval None
  */
static void COMMAND_ProgramFlashMemory(void)
{
  uint32_t total_size = 0x00;
  uint8_t readflag = TRUE;
  uint32_t bytesread;

  /* RAM Address Initialization */
  RamAddress = (uint32_t)&RAM_Buf;

  /* Erase address init */
  LastPGAddress = APPLICATION_ADDRESS;

  /* While file still contain data */
  while ((readflag == TRUE))
  {
    /* Read maximum 512 Kbyte from the selected file */
    f_read(&down_load_file, RAM_Buf, BUFFER_SIZE, (void *)&bytesread);

    /* Temp variable */
    TmpReadSize = bytesread;
    total_size += bytesread;

    /* The read data < "BUFFER_SIZE" Kbyte */
    if (TmpReadSize < BUFFER_SIZE)
    {
      readflag = FALSE;
    }

    /* Program flash memory */
    for (uint32_t i = 0; i < TmpReadSize; i += 4)
    {
      /* Write word into flash memory */
      if (FLASH_If_Write((LastPGAddress + i), *(uint32_t *)(RamAddress + i)) != 0x00)
      {
        /* Flash programming error: Turn LED3 On and Toggle LED4 in infinite
         * loop */
        // BSP_LED_On(LED3);
        Fail_Handler();
      }
    }
    /* Update last programmed address value */
    LastPGAddress += TmpReadSize;
  }

  printf("total_size=%lu\n", total_size);
}

void find_bin_file(const char *name)
{
    FRESULT fr;     /* Return value */
    DIR dj;         /* Directory search object */
    FILINFO fno;    /* File information */

    fr = f_findfirst(&dj, &fno, "", name);  /* Start to search for photo files */

    while (fr == FR_OK && fno.fname[0]) {         /* Repeat while an item is found */
        printf("%s\n", fno.fname);                /* Display the object name */
        fr = f_findnext(&dj, &fno);               /* Search for next item */
    }

    f_closedir(&dj);
}

FRESULT find_file(const TCHAR* path, const TCHAR* ext, TCHAR* foundFile)
{
  DIR dir;
  FILINFO fileInfo;
  FRESULT res;
  int len=0;
  // Open directory
  res = f_opendir(&dir, path);
  if (res != FR_OK) {
      return res;
  }

  // Traverse directory
  while (1) {
      // Read directory entry
      res = f_readdir(&dir, &fileInfo);
      if (res != FR_OK || fileInfo.fname[0] == 0) {
          // End of directory or error, break loop
          break;
      }

      // Ignore hidden files and directories
      if (fileInfo.fattrib & AM_HID) {
          continue;
      }
      // printf("%s;%d;\n",fileInfo.fname,strlen(fileInfo.fname));
      // printf("%s;%d;\n",fileInfo.fname,sizeof(fileInfo.fname));
      
      // Check file extension
      //if (fileInfo.fattrib & AM_ARC && strcmp(fileInfo.fname + strlen(fileInfo.fname) - 3, ext) == 0) 
      len = strlen(fileInfo.fname);
      if (len >= 4 && strcasecmp(fileInfo.fname + len - 4, ext) == 0)         
      {
          // Print file name
          printf("%s\r\n", fileInfo.fname);
          strncpy(foundFile, fileInfo.fname, sizeof(fileInfo.fname));
          foundFile[sizeof(fileInfo.fname) - 1] = '\0';        
      }
  }

  // Close directory
  f_closedir(&dir);

  return FR_OK;
}

FRESULT listFiles(const TCHAR* path)
{
  DIR dir;
  FILINFO fileInfo;
  FRESULT res;

  // Open directory
  res = f_opendir(&dir, path);
  if (res != FR_OK) {
      return res;
  }

  // Traverse directory
  while (1) {
      // Read directory entry
      res = f_readdir(&dir, &fileInfo);
      if (res != FR_OK || fileInfo.fname[0] == 0) {
          // End of directory or error, break loop
          break;
      }

      // Ignore hidden files and directories
      if (fileInfo.fattrib & AM_HID) {
          continue;
      }

      // Print file name
      if (fileInfo.fattrib & AM_DIR) {
          // Directory
          printf("[%s]\r\n", fileInfo.fname);
      } else {
          // File
          printf("%s\r\n", fileInfo.fname);
      }
  }

  // Close directory
  f_closedir(&dir);

  return FR_OK;
}
 
// Find files with .bin extension in the root directory
// findFiles("/", "bin");
FRESULT findFiles(const TCHAR* path, const TCHAR* ext) 
{
  DIR dir;
  FILINFO fileInfo;
  FRESULT res;
  int len=0;
  // Open directory
  res = f_opendir(&dir, path);
  if (res != FR_OK) {
      return res;
  }

  // Traverse directory
  while (1) {
      // Read directory entry
      res = f_readdir(&dir, &fileInfo);
      if (res != FR_OK || fileInfo.fname[0] == 0) {
          // End of directory or error, break loop
          break;
      }

      // Ignore hidden files and directories
      if (fileInfo.fattrib & AM_HID) {
          continue;
      }
      printf("%s;%d;\n",fileInfo.fname,strlen(fileInfo.fname));
      printf("%s;%d;\n",fileInfo.fname,sizeof(fileInfo.fname));
      printf("ext:%s;%d;\n",ext,strlen(ext));
      // Check file extension
      //if (fileInfo.fattrib & AM_ARC && strcmp(fileInfo.fname + strlen(fileInfo.fname) - 3, ext) == 0) 
      len = strlen(fileInfo.fname);
      if (len >= 4 && strcasecmp(fileInfo.fname + len - 4, ext) == 0)         
      {
          // Print file name
          printf("%s\r\n", fileInfo.fname);
      }
  }

  // Close directory
  f_closedir(&dir);

  return FR_OK;
}
 
void openFile(const TCHAR* path)
{
  FIL file;
  FRESULT res;

  res = f_open(&file, path, FA_READ);
  if (res == FR_OK) {
      // open success
      // ...
      
      f_close(&file);
  } else {
      // open false
      // ...
  }
}

