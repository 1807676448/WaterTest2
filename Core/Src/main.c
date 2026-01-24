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
#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_aht20_bmp280.h"
#include "app_debug.h"
#include "app_ds18b20.h"
#include "app_ph_sensor.h"
#include "app_tds_sensor.h"
#include "app_turbidity.h"
#include "app_json_comm.h"
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
/* USER CODE BEGIN PFP */

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
  MX_ADC1_Init();
  MX_I2C2_Init();
  MX_TIM6_Init();
  MX_TIM7_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  // 1. 串口打印系统启动信息
  printf("\r\n--- Water Quality Monitor System Start ---\r\n");

  // 2. 初始化各传感器设备
  printf("Initializing Sensors...\r\n");

  // 初始化 TDS 传感器
  if (TDS_Init(&hadc1) == 0)
    printf("TDS Sensor: OK\r\n");
  else
    printf("TDS Sensor: FAILED\r\n");

  // 初始化 浊度 传感器
  Turbidity_Init(&hadc1);
  printf("Turbidity Sensor: OK\r\n");

  // 初始化 DS18B20 温度传感器
  if (DS18B20_Init() == 0)
    printf("DS18B20 Sensor: OK\r\n");
  else
    printf("DS18B20 Sensor: FAILED\r\n");

  // 初始化 AHT20 湿度/温度传感器
  if (AHT20_Init(&hi2c2) == 1)
    printf("AHT20 Sensor: OK\r\n");
  else
    printf("AHT20 Sensor: FAILED\r\n");

  // 初始化 BMP280 压力传感器
  if (BMP280_Init(&hi2c2) == 0x58)
    printf("BMP280 Sensor: OK\r\n");
  else
    printf("BMP280 Sensor: FAILED\r\n");

  // 初始化串口通信接收
  Comm_Init();

  printf("Sensors Initialization Finished.\r\n\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_send_time = 0;

  while (1)
  {
    // 非阻塞处理串口命令
    Comm_Process_Rx_Command();

    // 周期性执行数据采集和发送 (间隔由 report_interval 控制)
    if (HAL_GetTick() - last_send_time >= report_interval)
    {
      last_send_time = HAL_GetTick();

      // 1. 读取水质传感数据
      float ph_val = PH_Read_Median();
      float tds_val = TDS_Read_Corrected();
      float turbidity_val = Turbidity_Read_NTU();
      float water_temp = DS18B20_Get_Temp();

      // 2. 读取环境温湿度及气压数据
      uint32_t aht_data[2];
      float air_hum = 0, air_temp_aht = 0;
      if (AHT20_Read_CTdata(&hi2c2, aht_data) == 1)
      { // 检查成功返回1
        air_hum = (float)aht_data[0] / 1048576.0f * 100.0f;
        air_temp_aht = (float)aht_data[1] / 1048576.0f * 200.0f - 50.0f;
      }

      float pressure = 0, air_temp_bmp = 0, asl = 0;
      BMP280GetData(&hi2c2, &pressure, &air_temp_bmp, &asl);

      // 3. 数据发送（打印到串口）
      printf("--- Water Quality ---\r\n");
      printf("Water Temp: %.1f C\r\n", water_temp);
      printf("PH Value:   %.2f\r\n", ph_val);
      printf("TDS Value:  %.1f ppm\r\n", tds_val);
      printf("Turbidity:  %.1f NTU\r\n", turbidity_val);

      printf("--- Environment ---\r\n");
      printf("Air Temp:   %.1f C\r\n", air_temp_bmp);
      printf("Air Hum:    %.1f %%\r\n", air_hum);
      printf("Pressure:   %.1f hPa\r\n", pressure / 100.0f);
      printf("Altitude:   %.1f m\r\n", asl);
      printf("---------------------\r\n\r\n");

      // 4. 发送 JSON 数据到上位机 (USART2)
      Comm_Send_Sensor_Data(ph_val, tds_val, turbidity_val, water_temp, air_temp_bmp, air_hum, pressure, asl);

      /* USER CODE END WHILE */

      /* USER CODE BEGIN 3 */
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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
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
