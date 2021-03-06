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
#include "dma.h"
#include "gpio.h"
#include "i2c.h"
#include "spi.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "commands.h"
#include "encoders.h"
#include "keys.h"
#include "ws2812b.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef SLAVE_STATE I2C_SLAVE_STATE;
typedef SLAVE_COMMAND I2C_SLAVE_COMMAND;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static SlaveState slave_state = {
  .state = WAITING_FOR_MASTER,
  .tx_buffer = {.data = {0}, .size = 0},
};
static XferBuffer rx_buffer = {.data = {0}, .size = 0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef* hi2c)
{
  if (slave_state.state == WAITING_FOR_MASTER) {
    uint8_t req_len = handleSlaveCommand(&slave_state, rx_buffer.data[0]);
    if (hi2c->XferSize > 1) {
      handleSlaveCommandArgs(&slave_state, (ByteSpan){rx_buffer.data + 1, hi2c->XferSize - 1});
      return;
    }
    if (req_len != 0) {
      rx_buffer.size = req_len;
      if (HAL_I2C_Slave_Seq_Receive_IT(&hi2c1, rx_buffer.data, req_len, I2C_LAST_FRAME) != HAL_OK) {
        /* Transfer error in reception process */
        HAL_GPIO_TogglePin(GPIO_LED_PIN.port, GPIO_LED_PIN.pin);
        Error_Handler();
      }
    }
  } else if (slave_state.state == COMMAND) {
    handleSlaveCommandArgs(&slave_state, (ByteSpan){rx_buffer.data, rx_buffer.size});
  }
}
void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef* hi2c)
{
  slave_state.state = WAITING_FOR_MASTER;
  rx_buffer.size = 0;
  slave_state.tx_buffer.size = 0;
}
void HAL_I2C_AddrCallback(I2C_HandleTypeDef* hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
{
  if (AddrMatchCode == OTTO_I2C_ADDRESS) {
    if (TransferDirection == I2C_DIRECTION_TRANSMIT && slave_state.state == WAITING_FOR_MASTER) {
      rx_buffer.size = 1;
      if (HAL_I2C_Slave_Seq_Receive_IT(&hi2c1, rx_buffer.data, rx_buffer.size, I2C_FIRST_FRAME) != HAL_OK) {
        Error_Handler();
      }
      //} else if (TransferDirection == I2C_DIRECTION_RECEIVE) {
      //  if (HAL_I2C_Slave_Seq_Transmit_IT(&hi2c1, slave_state.tx_buffer.data, slave_state.tx_buffer.size,
      //  I2C_FIRST_FRAME) != HAL_OK) {
      //    Error_Handler();
      //  }
    }
  }
}
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi)
{
  // Disable all buffers after SPI write
  HAL_GPIO_WritePin(GPIO_SPI_CS_MCU_LED_DRUM.port, GPIO_SPI_CS_MCU_LED_DRUM.pin, 1);
  HAL_GPIO_WritePin(GPIO_SPI_CS_MCU_LED_FUNC.port, GPIO_SPI_CS_MCU_LED_FUNC.pin, 1);
  HAL_GPIO_WritePin(GPIO_SPI_CS_MCU_LED_SEQ.port, GPIO_SPI_CS_MCU_LED_SEQ.pin, 1);
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
  /* USER CODE BEGIN 1 */
  uint8_t i;
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
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(400); // Delay some milliseconds for LEDs to power on

  initWS2812B(&gLED_DRUM_STRING, &hspi1, GPIO_SPI_CS_MCU_LED_DRUM, 10);
  initWS2812B(&gLED_SEQ_STRING, &hspi1, GPIO_SPI_CS_MCU_LED_SEQ, 16);
  initWS2812B(&gLED_FUNC_STRING, &hspi1, GPIO_SPI_CS_MCU_LED_FUNC, 21);
  for (i = 0; i < NUM_LED_STRINGS; i++) {
    clearPixels(gLED_STRINGS[i]);
    showPixels(gLED_STRINGS[i]);
  }
  initEncoder(&gEncoders[0], GPIO_ENC_A_1, GPIO_ENC_B_1);
  initEncoder(&gEncoders[1], GPIO_ENC_A_2, GPIO_ENC_B_2);
  initEncoder(&gEncoders[2], GPIO_ENC_A_3, GPIO_ENC_B_3);
  initEncoder(&gEncoders[3], GPIO_ENC_A_4, GPIO_ENC_B_4);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    if (HAL_I2C_GetState(&hi2c1) == HAL_I2C_STATE_LISTEN && slave_state.state == READY_TO_RESPOND) {
      if (HAL_I2C_DisableListen_IT(&hi2c1) != HAL_OK) {
        /* Transfer error in reception process */
        HAL_GPIO_TogglePin(GPIO_LED_PIN.port, GPIO_LED_PIN.pin);
        Error_Handler();
      }
    }
    if (HAL_I2C_GetState(&hi2c1) == HAL_I2C_STATE_READY) {
      switch (slave_state.state) {
        case COMMAND:
        case WAITING_FOR_MASTER:
          // case READY_TO_RESPOND:
          if (HAL_I2C_EnableListen_IT(&hi2c1) != HAL_OK) {
            /* Transfer error in reception process */
            HAL_GPIO_TogglePin(GPIO_LED_PIN.port, GPIO_LED_PIN.pin);
            Error_Handler();
          }
          break;
        case READY_TO_RESPOND:
          if (HAL_I2C_Slave_Transmit_IT(&hi2c1, slave_state.tx_buffer.data, slave_state.tx_buffer.size) != HAL_OK) {
            /* Transfer error in transmission process */
            HAL_GPIO_TogglePin(GPIO_LED_PIN.port, GPIO_LED_PIN.pin);
            Error_Handler();
          }
          break;
        default: break;
      }
    }

    else if (HAL_I2C_GetState(&hi2c1) == HAL_I2C_STATE_ERROR) {
      switch (HAL_I2C_GetError(&hi2c1)) {
        case HAL_I2C_ERROR_AF: __HAL_I2C_CLEAR_FLAG(&hi2c1, I2C_FLAG_AF); break;
        default: break;
      }
    }

    showPixels(&gLED_FUNC_STRING);
    showPixels(&gLED_DRUM_STRING);
    showPixels(&gLED_SEQ_STRING);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the CPU, AHB and APB busses clocks
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
    Error_Handler();
  }
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
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number */
  abort();
  // printf("Wrong parameters value: file %s on line %d\r\n", file, line);
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
