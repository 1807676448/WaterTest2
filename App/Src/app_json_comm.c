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
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return;

    cJSON_AddNumberToObject(root, "device_id", COMM_DEVICE_ID);
    
    // 添加数据对象
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "ph", ph);
    cJSON_AddNumberToObject(data, "tds", tds);
    cJSON_AddNumberToObject(data, "turbidity", turb);
    cJSON_AddNumberToObject(data, "water_temp", w_temp);
    cJSON_AddNumberToObject(data, "air_temp", a_temp);
    cJSON_AddNumberToObject(data, "humidity", a_hum);
    cJSON_AddNumberToObject(data, "pressure", pres);
    cJSON_AddNumberToObject(data, "altitude", asl);

    cJSON_AddItemToObject(root, "data", data);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str != NULL)
    {
        // 发送数据，追加换行符
        HAL_UART_Transmit(COMM_UART_HANDLE, (uint8_t *)json_str, strlen(json_str), 1000);
        HAL_UART_Transmit(COMM_UART_HANDLE, (uint8_t *)"\r\n", 2, 100);
        free(json_str); // 必须释放
    }

    cJSON_Delete(root);
}

// 辅助函数：发送简单的响应消息
static void Comm_Send_Response(const char* status, const char* message)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "device_id", COMM_DEVICE_ID);
    cJSON_AddStringToObject(root, "status", status);
    if(message != NULL)
        cJSON_AddStringToObject(root, "message", message);
    
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str != NULL) {
        HAL_UART_Transmit(COMM_UART_HANDLE, (uint8_t *)json_str, strlen(json_str), 1000);
        HAL_UART_Transmit(COMM_UART_HANDLE, (uint8_t *)"\r\n", 2, 100);
        free(json_str);
    }
    cJSON_Delete(root);
}

void Comm_Process_Rx_Command(void)
{
    if (data_ready_flag)
    {
        // 尝试解析JSON
        cJSON *root = cJSON_Parse((char *)rx_buffer);
        if (root != NULL)
        {
            cJSON *id_item = cJSON_GetObjectItem(root, "device_id");
            cJSON *cmd_item = cJSON_GetObjectItem(root, "command");
            cJSON *val_item = cJSON_GetObjectItem(root, "value");

            if (cJSON_IsNumber(id_item) && id_item->valueint == COMM_DEVICE_ID)
            {
                if (cJSON_IsString(cmd_item))
                {
                    // 1. 重启设备
                    if (strcmp(cmd_item->valuestring, "REBOOT") == 0)
                    {
                        Comm_Send_Response("OK", "Rebooting...");
                        HAL_Delay(100);
                        HAL_NVIC_SystemReset();
                    }
                    // 2. 设置上报时间间隔 (ms)
                    else if (strcmp(cmd_item->valuestring, "SET_INTERVAL") == 0)
                    {
                        if (cJSON_IsNumber(val_item))
                        {
                            report_interval = val_item->valueint;
                            if (report_interval < 100) report_interval = 100; // 最小限制
                            Comm_Send_Response("OK", "Interval Updated");
                        }
                        else
                        {
                            Comm_Send_Response("ERROR", "Invalid Value");
                        }
                    }
                    // 3. 设置 TDS 校准系数 K
                    else if (strcmp(cmd_item->valuestring, "SET_TDS_K") == 0)
                    {
                        if (cJSON_IsNumber(val_item))
                        {
                            TDS_Set_K_Value((float)val_item->valuedouble);
                            Comm_Send_Response("OK", "TDS K Value Updated");
                        }
                        else
                        {
                            Comm_Send_Response("ERROR", "Invalid Value");
                        }
                    }
                    // 4. 获取设备状态
                    else if (strcmp(cmd_item->valuestring, "GET_STATUS") == 0)
                    {
                        Comm_Send_Response("OK", "Running");
                    }
                    else
                    {
                        Comm_Send_Response("ERROR", "Unknown Command");
                    }
                }
            }
            cJSON_Delete(root);
        }
        else
        {
             // JSON 解析失败 (可选：打印错误信息)
             // Comm_Send_Response("ERROR", "Invalid JSON");
        }
        
        // 处理完毕，重置缓冲区
        data_ready_flag = 0;
        rx_index = 0;
        memset(rx_buffer, 0, COMM_RX_BUFFER_SIZE);
    }
}
