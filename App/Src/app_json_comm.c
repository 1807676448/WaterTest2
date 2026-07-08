#include "app_json_comm.h"
#include "usart.h"
#include "app_tds_sensor.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// 全局变量定义
uint32_t report_interval = 1500; // 默认5秒上报一次

// 外部引用串口句柄
extern UART_HandleTypeDef huart2;

// 外部引用静默标志 (定义在 main.c)
extern volatile uint8_t tx_muted;

void Comm_Init(void)
{
    // 初始化由 main.c 中的中断回调处理
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
// 注意：调用前请确保 tx_muted == 0，否则数据不会被发送
void Comm_Send_Response(const char *status)
{
    if (!tx_muted)
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
}