#ifndef __APP_JSON_COMM_H
#define __APP_JSON_COMM_H

#include "main.h"
#include "cJSON.h"

// ================== 配置宏定义 ==================
#define COMM_DEVICE_ID      3           // 设备ID
#define COMM_UART_HANDLE    &huart2     // 通信使用的串口句柄
#define COMM_RX_BUFFER_SIZE 256         // 接收缓冲区大小

// 全局变量声明：上报时间间隔 (ms)
extern uint32_t report_interval;

// ================== 函数声明 ==================
/**
 * @brief 初始化通信模块（启动接收）
 */
void Comm_Init(void);

/**
 * @brief 发送传感器数据
 * @param ph pH值
 * @param tds TDS值
 * @param turb 浊度
 * @param w_temp 水温
 * @param a_temp 空气温度
 * @param a_hum 空气湿度
 * @param pres 气压
 * @param asl 海拔
 */
void Comm_Send_Sensor_Data(float ph, float tds, float turb, float w_temp, float a_temp, float a_hum, float pres, float asl);

/**
 * @brief 处理接收到的数据 (在主循环中调用)
 */

// void Comm_Process_Rx_Command(void);


void Comm_Send_Response(const char *status);
#endif
