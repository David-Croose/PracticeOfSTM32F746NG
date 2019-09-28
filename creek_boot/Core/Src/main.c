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
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "crc.h"
#include "fatfs.h"
#include "sdmmc.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
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

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
extern void GRAPHICS_HW_Init(void);
extern void GRAPHICS_Init(void);
extern void GRAPHICS_MainTask(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
extern TIM_HandleTypeDef htim6;
extern LTDC_HandleTypeDef hltdc;
extern DMA2D_HandleTypeDef hdma2d;

int32_t is_sdram_work_well(uint32_t addr, uint32_t size) {
    uint32_t i, j;
    uint8_t * const p = (uint8_t *)addr;
    uint32_t bytes = size;
    uint32_t halfwords = size / 2;
    uint32_t words = size / 4;
    uint32_t doubles = size / 8;

    __HAL_TIM_DISABLE_IT(&htim6, TIM_IT_UPDATE);

    // testing access whole SDRAM in unit: byte
    for (i = 0; i < bytes; i++) {
    	p[i] = i;
    }
    for (j = 0, i = 0; i < bytes; i++) {
        if (p[i] != j) {
        	goto error;
        }
        if (++j >= 256) {
        	j = 0;
        }
    }

    // testing access whole SDRAM in unit: halfword
    for (i = 0; i < halfwords; i++) {
    	*(uint16_t *)&p[i * 2] = i;
    }
    for (j = 0, i = 0; i < halfwords; i++) {
        if (*(uint16_t *)&p[i * 2] != j) {
        	goto error;
        }
        if (++j >= 65536) {
        	j = 0;
        }
    }

    // testing access whole SDRAM in unit: word
    for (i = 0; i < words; i++) {
    	*(uint32_t *)&p[i * 4] = i;
    }
    for (i = 0; i < words; i++) {
        if (*(uint32_t *)&p[i * 4] != i) {
        	goto error;
        }
    }

    // testing access whole SDRAM in unit: double
    for (i = 0; i < doubles; i++) {
    	*(uint64_t *)&p[i * 8] = i;
    }
    for (i = 0; i < doubles; i++) {
        if (*(uint64_t *)&p[i * 8] != i) {
        	goto error;
        }
    }

	__HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
    return 1;
error:
	__HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
	return 0;
}

static void move_and_jump(void) {
#define USE_LINUX       (0)
#define DTB_ADDR        (0xC0000000)
#define KER_ADDR        (0xC0000000 + 32 * 1024)
#define NEW_ADDR(x)		((x) - 0x60000000)
#define SDRAM_SIZE		(8 * 1024 * 1024)	/* bytes */
#define ERR(x)			if ((x)) { printf("error in line: %d, res=0x%x", __LINE__, (x)); return; }

    static FATFS fs;
    static FIL fil;
    FRESULT res;
    unsigned int br;
    unsigned char *dtb = (unsigned char *)DTB_ADDR;
    unsigned char *ker = (unsigned char *)KER_ADDR;
	typedef void (*func_t)(void);		// TODO  don't forget to revert it back
    static func_t kernel;

    printf("start...\r\n");

    // check SDRAM
    printf("checking SDRAM...\r\n");
    if (!is_sdram_work_well(DTB_ADDR, SDRAM_SIZE)) {
    	printf("error, the SDRAM read write check is not passed\r\n");
    	return;
    }

    // copy devicetree from SD card to SDRAM
    res = f_mount(&fs, "0:", 1); ERR(res);
    res = f_open(&fil, "0:/devicetree.dtb", FA_READ); ERR(res);
    printf("copy devicetree(%dB) to 0x%08x...\r\n", (int)f_size(&fil), DTB_ADDR);
    res = f_read(&fil, dtb, f_size(&fil), &br); ERR(res);
    res = f_close(&fil); ERR(res);

    // copy kernel from SD card to SDRAM
    res = f_open(&fil, "0:/kernel.bin", FA_READ); ERR(res);
    printf("copy kernel(%dB) to 0x%08x...\r\n", (int)f_size(&fil), KER_ADDR);
    res = f_read(&fil, ker, f_size(&fil), &br); ERR(res);
    res = f_close(&fil); ERR(res);

    // remap the SDRAM address from 0xC0000000 to 0x60000000 or the code in
    // SDRAM won't be executed. on the other hand, you could also set the MPU
    // to do this
    // TODO  if you remap first then copy code into SDRAM, the f_read will fail, I
    // don't know why
    HAL_EnableFMCMemorySwapping();
    HAL_Delay(1000);			// the swaping of SDRAM and NOR/RAM takes some time

    // jump to kernel
    printf("everthing is ok, now jump to kernel\r\n");
    __HAL_TIM_DISABLE_IT(&htim6, TIM_IT_UPDATE);
    HAL_LTDC_DeInit(&hltdc);
    HAL_DMA2D_MspDeInit(&hdma2d);

    /*
     * you could not disable Icache and Dcache, or the jumping will fail. and you
     * must enable them in the beginning of main(), or the SDMMC work error
     *
     * SCB_CleanDCache();
     * SCB_DisableDCache();
     * SCB_InvalidateICache();
     * SCB_DisableICache();
     */

#if (USE_LINUX == 1)
    kernel = (func_t)(NEW_ADDR(KER_ADDR | 1));
    kernel();
#else
    kernel = (func_t)(*(unsigned int *)NEW_ADDR(KER_ADDR + 4));
    __set_MSP(*(unsigned int *)NEW_ADDR(KER_ADDR));
    kernel();
#endif
	printf("fatal error\r\n");
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
  

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

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
  MX_CRC_Init();
  MX_SDMMC1_SD_Init();
  MX_FATFS_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

/* Initialise the graphical hardware */
  GRAPHICS_HW_Init();

  /* Initialise the graphical stack engine */
  GRAPHICS_Init();
  
  /* Graphic application */  
  GRAPHICS_MainTask();

  move_and_jump();
  /* Infinite loop */
  for(;;);
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Configure the main internal regulator output voltage 
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 432;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Activate the Over-Drive mode 
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC|RCC_PERIPHCLK_USART1
                              |RCC_PERIPHCLK_SDMMC1|RCC_PERIPHCLK_CLK48;
  PeriphClkInitStruct.PLLSAI.PLLSAIN = 384;
  PeriphClkInitStruct.PLLSAI.PLLSAIR = 5;
  PeriphClkInitStruct.PLLSAI.PLLSAIQ = 2;
  PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV8;
  PeriphClkInitStruct.PLLSAIDivQ = 1;
  PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_8;
  PeriphClkInitStruct.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48SOURCE_PLLSAIP;
  PeriphClkInitStruct.Sdmmc1ClockSelection = RCC_SDMMC1CLKSOURCE_CLK48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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
