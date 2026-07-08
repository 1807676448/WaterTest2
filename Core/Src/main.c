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
#include <stdio.h>
#include <string.h>
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
#define RX_BUFFER_SIZE 64

// 调试指令格式: 0xAA 0xAA 0xBB 0xBB (4字节序列)
#define DEBUG_CMD_BYTE0 0xAA
#define DEBUG_CMD_BYTE1 0xAA
#define DEBUG_CMD_BYTE2 0xBB
#define DEBUG_CMD_BYTE3 0xBB
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// USART2接收相关变量
uint8_t usart2_rx_buffer[RX_BUFFER_SIZE];
uint16_t usart2_rx_index = 0;
uint8_t usart2_rx_complete = 0;
uint8_t cmd_rx_state = 0;       // 指令状态: 0=等AA1, 1=等AA2, 2=等BB1, 3=等BB2
uint8_t cmd_toggle_state = 0;   // 调试切换指令: 0=等CC1, 1=等CC2, 2=等CC3
uint8_t cmd_mute_state = 0;     // 静默指令: 0=等DD1, 1=等DD2, 2=等DD3
uint8_t cmd_unmute_state = 0;   // 恢复指令: 0=等EE1, 1=等EE2, 2=等EE3

volatile uint8_t tx_muted = 0;  // 0=允许USART2发送, 1=静默(禁止发送) (中断中修改)

uint8_t TestStart = 0;
uint8_t debug_mode = 0;         // 0=正常模式(周期检测), 1=调试模式(指令触发检测)

uint64_t tick_counter = 0;
uint64_t tick_heart = 0;

// 总线空闲检测：记录最后一次收到字节的时刻
uint64_t tick_last_rx = 0;

// 中断触发计数 (调试用，已注释)
// volatile uint32_t usart2_int_count = 0;

// 指令触发计数 (调试用，已注释)
// uint32_t cmd_trigger_count = 0;

// 上次发送计数器数据的时间 (调试用，已注释)
// uint32_t last_count_send_time = 0;
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

  // 启动USART2中断接收 (单字节模式，用于指令序列检测)
  usart2_rx_index = 0;
  usart2_rx_complete = 0;
  HAL_UART_Receive_IT(&huart2, &usart2_rx_buffer[0], 1);

  // 初始化计数变量 (调试用，已注释)
  // usart2_int_count = 0;
  // cmd_trigger_count = 0;
  // last_count_send_time = HAL_GetTick();
  tick_counter = HAL_GetTick();
  tick_heart = HAL_GetTick();
  tick_last_rx = HAL_GetTick();

  printf("Sensors Initialization Finished.\r\n\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_send_time = 0;

  while (1)
  {
    // 周期性执行数据采集和发送 (间隔由 report_interval 控制)
    if (HAL_GetTick() - last_send_time >= report_interval)
    {
      last_send_time = HAL_GetTick();

      // 1. 读取水质传感数据
      float ph_val = PH_Read_Median();
      float tds_val = TDS_Read_Corrected();
      float water_temp = DS18B20_Get_Temp();

      float turbidity_val = Turbidity_Read_NTU(water_temp);

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

      // 4. 发送 JSON 数据到上位机 (USART2)，静默时不发送
      if (!tx_muted)
      {
        Comm_Send_Sensor_Data(ph_val, tds_val, turbidity_val, water_temp, air_temp_bmp, air_hum, pressure, asl);
      }
      
      // Comm_Send_Response("Active");
    }

    // 调试模式：由串口指令触发单次检测
    if (debug_mode && TestStart)
    {
      TestStart = 0;
      // 调试模式下可在此处添加单次触发的操作
      printf("Debug trigger received.\r\n");
      if (!tx_muted)
      {
        Comm_Send_Response("DebugTrigger");
      }
    }

    // 每隔10秒通过USART2发送中断计数信息 (调试完毕，已注释)
    // if (HAL_GetTick() - last_count_send_time >= 10000)
    // {
    //   last_count_send_time = HAL_GetTick();
    //
    //   if (!tx_muted)
    //   {
    //     char count_msg[128];
    //     int len = snprintf(count_msg, sizeof(count_msg),
    //                       "{\"device_id\":%d,\"int_count\":%lu,\"cmd_count\":%lu,\"status\":\"count_report\"}\r\n",
    //                       COMM_DEVICE_ID, usart2_int_count, cmd_trigger_count);
    //     HAL_UART_Transmit(&huart2, (uint8_t *)count_msg, len, 1000);
    //   }
    // }
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
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
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

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    // 中断触发计数加1 (调试用，已注释)
    // usart2_int_count++;

    tick_last_rx = HAL_GetTick();  // 更新最后收到字节的时刻
    uint8_t received_byte = usart2_rx_buffer[usart2_rx_index];

    // 指令格式: 0xAA 0xAA 0xBB 0xBB (4字节序列状态机)
    switch (cmd_rx_state)
    {
    case 0:  // 等待第一个 0xAA
      if (received_byte == DEBUG_CMD_BYTE0)
      {
        cmd_rx_state = 1;
      }
      break;

    case 1:  // 等待第二个 0xAA
      if (received_byte == DEBUG_CMD_BYTE1)
      {
        cmd_rx_state = 2;
      }
      else
      {
        cmd_rx_state = 0;
      }
      break;

    case 2:  // 等待第一个 0xBB
      if (received_byte == DEBUG_CMD_BYTE2)
      {
        cmd_rx_state = 3;
      }
      else
      {
        cmd_rx_state = 0;
      }
      break;

    case 3:  // 等待第二个 0xBB → 指令完成
      if (received_byte == DEBUG_CMD_BYTE3)
      {
        TestStart = 1;        // 触发单次检测
        // cmd_trigger_count++;  // 指令触发计数加1 (调试用，已注释)
      }
      cmd_rx_state = 0;  // 无论匹配与否, 均重置状态
      break;
    }

    // 调试模式切换指令: 0xCC 0xCC 0xCC (3字节序列)
    switch (cmd_toggle_state)
    {
    case 0:  // 等待第一个 0xCC
      if (received_byte == 0xCC)
      {
        cmd_toggle_state = 1;
      }
      break;

    case 1:  // 等待第二个 0xCC
      if (received_byte == 0xCC)
      {
        cmd_toggle_state = 2;
      }
      else
      {
        cmd_toggle_state = 0;
      }
      break;

    case 2:  // 等待第三个 0xCC → 切换调试模式
      if (received_byte == 0xCC)
      {
        debug_mode = !debug_mode;
      }
      cmd_toggle_state = 0;
      break;
    }

    // 静默开启指令: 0xDD 0xDD 0xDD
    switch (cmd_mute_state)
    {
    case 0:
      if (received_byte == 0xDD) { cmd_mute_state = 1; }
      break;
    case 1:
      if (received_byte == 0xDD) { cmd_mute_state = 2; }
      else { cmd_mute_state = 0; }
      break;
    case 2:
      if (received_byte == 0xDD) { tx_muted = 1; }
      cmd_mute_state = 0;
      break;
    }

    // 静默关闭指令: 0xEE 0xEE 0xEE
    switch (cmd_unmute_state)
    {
    case 0:
      if (received_byte == 0xEE) { cmd_unmute_state = 1; }
      break;
    case 1:
      if (received_byte == 0xEE) { cmd_unmute_state = 2; }
      else { cmd_unmute_state = 0; }
      break;
    case 2:
      if (received_byte == 0xEE) { tx_muted = 0; }
      cmd_unmute_state = 0;
      break;
    }

    // 继续接收下一个字节
    if (usart2_rx_index < RX_BUFFER_SIZE - 1)
    {
      usart2_rx_index++;
      HAL_UART_Receive_IT(&huart2, &usart2_rx_buffer[usart2_rx_index], 1);
    }
    else
    {
      // 缓冲区满，重置索引
      usart2_rx_index = 0;
      HAL_UART_Receive_IT(&huart2, &usart2_rx_buffer[usart2_rx_index], 1);
    }
  }
}

// 错误回调
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    // 清除错误标志
    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
    __HAL_UART_CLEAR_PEFLAG(huart);

    // 重新启动中断接收
    HAL_UART_Receive_IT(&huart2, &usart2_rx_buffer[0], 1);
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
