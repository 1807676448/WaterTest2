#include "app_json_comm.h"
#include "usart.h"
#include "app_tds_sensor.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// 全局变量定义
uint32_t report_interval = 1500; // 默认5秒上报一次

// 接收缓冲区
static uint8_t rx_buffer[COMM_RX_BUFFER_SIZE];
static uint8_t rx_byte;
static uint16_t rx_index = 0;
static uint8_t data_ready_flag = 0;

// 外部引用串口句柄
extern UART_HandleTypeDef huart2;

void Comm_Init(void)
{
    // 启动接收中断，每次接收一个字节
    HAL_UART_Receive_IT(COMM_UART_HANDLE, &rx_byte, 1);
}

// 串口接收中断回调处理
// 注意：如果工程中其他地方也定义了此回调，需要合并逻辑
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        // 简单处理：收到换行符或缓冲区满视为一帧结束
        // 兼容 \n 或 \r\n 或 } (JSON结束)
        // 这里采用最简单的行缓冲机制，或者以 } 结尾判断
        // 为了可靠性，单纯存入buffer，主循环解析。这里以 \n 作为结束符示例，实际JSON可能不带回车。
        // 改进：一直累积直到 buffer 满或者特定结束条件。
        // 假设命令以 \n 结束

        if (rx_index < COMM_RX_BUFFER_SIZE - 1)
        {
            rx_buffer[rx_index++] = rx_byte;

            // 收到换行符，标记接收完成
            if (rx_byte == '\n')
            {
                rx_buffer[rx_index] = '\0';
                data_ready_flag = 1;
                // 注意：这里没有重置 rx_index，依靠主循环处理完后重置
            }
        }
        else
        {
            // 溢出保护
            rx_index = 0;
            rx_buffer[rx_index++] = rx_byte;
        }

        // 继续接收
        HAL_UART_Receive_IT(COMM_UART_HANDLE, &rx_byte, 1);
    }
}

void Comm_Send_Sensor_Data(float ph, float tds, float turb, float w_temp, float a_temp, float a_hum, float pres, float asl)
{
    /* Buffer for JSON string. Ensure it's large enough for all data. */
    char tx_buffer[256];

    /* Format the data into a JSON string */
    int len = snprintf(tx_buffer, sizeof(tx_buffer),
                       "{\"device_id\":%d,\"pH\":%.2f,\"TDS\":%.1f,\"Tur\":%.1f,\"Tem\":%.1f,\"air_temp\":%.1f,\"air_hum\":%.1f,\"pressure\":%.1f,\"altitude\":%.1f,\"device_id\":%d,\"status\":\"%s\"}\r\n",
                       COMM_DEVICE_ID, ph, tds, turb, w_temp, a_temp, a_hum, pres, asl, COMM_DEVICE_ID, "Active");

    if (len > 0)
    {
        /* Send JSON string via USART2 */
        HAL_UART_Transmit(&huart2, (uint8_t *)tx_buffer, (uint16_t)len, 1000);
    }
}

// 辅助函数：发送简单的响应消息
void Comm_Send_Response(const char *status)
{
    char tx_buffer[64];
    int len;
    len = snprintf(tx_buffer, sizeof(tx_buffer),
                   "{\"device_id\":%d,\"status\":\"%s\"}\r\n",
                   COMM_DEVICE_ID, status);
    if (len > 0)
    {
        HAL_UART_Transmit(COMM_UART_HANDLE, (uint8_t *)tx_buffer, (uint16_t)len, 1000);
    }
}