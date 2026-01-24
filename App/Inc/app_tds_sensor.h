#ifndef __TDS_H
#define __TDS_H

#include "main.h"
#include <stdint.h>
#include <math.h>
#include <stdio.h>

typedef struct
{
    float raw_voltage;      // 原始电压值
    float raw_tds;          // 原始TDS值(未温度校正)
    float temperature;      // 当前温度
    float tds_corrected;    // 温度校正后的TDS值
    float k_value;          // 校准系数
} TDS_Data_TypeDef;

/**
 * @brief TDS初始化函数，配置ADC通道和DS18B20
 * @param hadc: ADC句柄指针
 * @return 返回0表示成功，非0表示失败
 */
uint8_t TDS_Init(ADC_HandleTypeDef *hadc);

/**
 * @brief 读取原始TDS值（未温度校正）
 * @return 返回TDS值(ppm)
 */
float TDS_Read_Raw(void);

/**
 * @brief 读取温度校正后的TDS值
 * @return 返回校正后的TDS值(ppm)
 */
float TDS_Read_Corrected(void);

/**
 * @brief 获取完整的TDS数据结构
 * @return 返回包含所有数据的TDS_Data_TypeDef结构体
 */
TDS_Data_TypeDef TDS_Get_Data(void);

/**
 * @brief 设置TDS校准系数
 * @param k_value: 校准系数 K = TDS标准值 / TDS测量值
 */
void TDS_Set_K_Value(float k_value);

/**
 * @brief 获取当前的K值
 * @return 返回当前的校准系数
 */
float TDS_Get_K_Value(void);

#endif /* __TDS_H */
