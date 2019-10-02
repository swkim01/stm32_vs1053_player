/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#define U8X8_USE_PINS
#include "u8g2.h"
#include "filemanager.h"
#include "player.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
//#define BUTTON_PLAY     A0
//#define BUTTON_DOWN     A1
//#define BUTTON_UP       A2
//#define BUTTON_STOP     A3
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
FATFS fs;

typedef enum {
  FILE_MODE,
  PLAY_MODE
} run_state;

u8g2_t u8g2;

DIR curDir;
uint8_t curSel = 1;
char title[31], album[31], artist[31];
char title8[40], album8[40], artist8[40];
int tlen, txpos=128;
int sec, seconds;
unsigned long timeVal;
char timebuf[10]="00:00";
run_state rstate = FILE_MODE;
int volume = 50;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
#ifdef __GNUC__
  /* With GCC/RAISONANCE, small printf (option LD Linker->Libraries->Small printf
     set to 'Yes') calls __io_putchar() */
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */

uint8_t u8x8_gpio_and_delay_stm32_sw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
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
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_FATFS_Init();
  /* USER CODE BEGIN 2 */

  /* Wait for SD module reset */
  HAL_Delay(500);

  /* Setup U8G2 for SSD1306 I2C Module */
  u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_sw_i2c, u8x8_gpio_and_delay_stm32_sw_i2c);
  u8g2_InitDisplay(&u8g2);
  u8g2_SetPowerSave(&u8g2, 0);
  u8g2_SetMenuSelectPin(&u8g2, U8X8_PIN_MENU_SELECT);
  u8g2_SetMenuDownPin(&u8g2, U8X8_PIN_MENU_DOWN);
  u8g2_SetMenuUpPin(&u8g2, U8X8_PIN_MENU_UP);
  u8g2_SetMenuHomePin(&u8g2, U8X8_PIN_MENU_HOME);

  u8g2_SetFont(&u8g2, u8g2_font_unifont_t_korean2);
  //u8g2_SetFont(&u8g1, u8g2_font_unifont_t_korean_NanumGothicCoding_16);
  u8g2_SetFontDirection(&u8g2, 0);

  /* Initialize VS1053 MP3 Module */
  player_init();
  printf("VS1053 found\n");

  player_sinetest(0x44, 500);    // Make a tone to indicate VS1053 is working
  timeVal = HAL_GetTick();

  /* Mount SD Card */
  if(f_mount(&fs, "", 0) != FR_OK)
    Error_Handler();

  /* List files */
  f_opendir(&curDir, curDirPath);
  printDirectory(&curDir, 0);
  f_rewinddir(&curDir);

  /* Set volume for left, right channels. lower numbers == louder volume! */
  vs10xx_set_volume(volume);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    if (rstate == FILE_MODE) {
      /* When File Mode, display file list on OLED */
      int filecount = getFileList(&curDir);
      curSel = u8g2_UserInterfaceSelectionList(&u8g2, curDirPath, curSel, fileNameList);
      if (curSel == 0) { // Move to Prev/Up directory
        f_closedir(&curDir);
        char *last = strrchr(curDirPath, '/');
        if (last != NULL) last[last!=curDirPath?0:1] = '\0';
        f_opendir(&curDir, curDirPath);
      }
      else {
        /* Select a File */
        strcpy(curFilePath,curDirPath);
        if (strcmp(curFilePath, "/"))
          strcat(curFilePath,"/");
        strcat(curFilePath,fileList[curSel-1]);

        FILINFO finfo;
        f_stat(curFilePath, &finfo);
        if (finfo.fattrib & AM_DIR) { // Go to the subdirectory
          f_closedir(&curDir);
          strcpy(curDirPath, curFilePath);
          f_opendir(&curDir, curDirPath);
          curSel = 1;
        } else {
          /* Select the mp3 file and change to play mode */
          if (strstr(finfo.fname, ".mp3") || strstr(finfo.fname, ".MP3")) {
            FIL curFile;
            f_open(&curFile, finfo.fname, FA_READ);
            getID3(&curFile, title, artist, album);
            ucs_to_utf8(title, title8);
            ucs_to_utf8(artist, artist8);
            ucs_to_utf8(album, album8);
            tlen = strlen(title8);
            seconds = getMP3Info(&curFile);
            printf("%s %s %s\n", title8, artist8, album8);
            printf("%d...\n", seconds);
            f_close(&curFile);
            player_play(curFilePath);
            rstate = PLAY_MODE;
            sec = 0;
          }
        }
      }
      HAL_Delay(100);
    }

    else {
      /* When Play Mode, The File is playing in the background */
      if (HAL_GetTick() - timeVal >= 1000) {
        sec++;
        sprintf(timebuf,"%02d:%02d", sec/60, sec%60);
        timeVal = HAL_GetTick();
      }
      if (playerState == PS_PAUSE)
        timeVal = HAL_GetTick();

      /* Display artist, album, song title on OLED */
      txpos = (txpos+tlen*8>0)?txpos-5:128;
      u8g2_FirstPage(&u8g2);
      do {
        u8g2_DrawUTF8(&u8g2, 0, 15, "Playing...");
        u8g2_DrawUTF8(&u8g2, 0, 30, artist8);
        u8g2_DrawUTF8(&u8g2, 60, 30, album8);
        u8g2_DrawUTF8(&u8g2, txpos, 45, title8);
        u8g2_DrawRFrame(&u8g2, 0,50,80,10,1);
        u8g2_DrawBox(&u8g2, 0,50,(80.0*sec/seconds),10);
        u8g2_DrawUTF8(&u8g2, 84, 60, timebuf);
      } while ( u8g2_NextPage(&u8g2) );

      if (playerState == PS_STOP) {
        printf("Done playing music\n");
        HAL_Delay(10);  // we're done! do nothing...
        rstate = FILE_MODE;
      }

      /* if we get BUTTON_STOP, stop! */
      if (!HAL_GPIO_ReadPin(BUTTON_STOP_GPIO_Port, BUTTON_STOP_Pin)) {
        player_stop();
        rstate = FILE_MODE;
      }

      /* if we get BUTTON_PLAY, pause/unpause! */
      if (!HAL_GPIO_ReadPin(BUTTON_PLAY_GPIO_Port, BUTTON_PLAY_Pin)) {
        if (playerState == PS_PLAY) {
          printf("Paused\n");
          player_pause(TRUE);
        } else if (playerState == PS_PAUSE) {
          printf("Resumed\n");
          player_pause(FALSE);
        }
      }

      /* if we get BUTTON_UP, volume up! */
      if (!HAL_GPIO_ReadPin(BUTTON_UP_GPIO_Port, BUTTON_UP_Pin)) {
        // Set volume for left, right channels. lower numbers == louder volume!
        volume -= 5;
        if (volume < 0) volume = 0;
        vs10xx_set_volume(volume);
      }

      /* if we get BUTTON_DOWN, volume down! */
      if (!HAL_GPIO_ReadPin(BUTTON_DOWN_GPIO_Port, BUTTON_DOWN_Pin)) {
        // Set volume for left, right channels. lower numbers == louder volume!
        volume += 5;
        if (volume > 100) volume = 100;
        vs10xx_set_volume(volume);
      }
      HAL_Delay(100);
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 360;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
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
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
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

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(XCS_GPIO_Port, XCS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, XDCS_Pin|XRESET_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BUTTON_PLAY_Pin BUTTON_DOWN_Pin BUTTON_UP_Pin */
  GPIO_InitStruct.Pin = BUTTON_PLAY_Pin|BUTTON_DOWN_Pin|BUTTON_UP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : BUTTON_STOP_Pin */
  GPIO_InitStruct.Pin = BUTTON_STOP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BUTTON_STOP_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : XCS_Pin */
  GPIO_InitStruct.Pin = XCS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(XCS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SD_CS_Pin */
  GPIO_InitStruct.Pin = SD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SD_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : XDCS_Pin XRESET_Pin */
  GPIO_InitStruct.Pin = XDCS_Pin|XRESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : DREQ_Pin */
  GPIO_InitStruct.Pin = DREQ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(DREQ_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

}

/* USER CODE BEGIN 4 */
/**
  * @brief  Retargets the C library printf function to the USART.
  * @param  None
  * @retval None
  */
PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the EVAL_COM1 and Loop until the end of transmission */
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);

  return ch;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    /* If mp3 buffer is ready for data, feed data */
    if (GPIO_Pin == DREQ_Pin) {
        player_feedbuffer();
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
