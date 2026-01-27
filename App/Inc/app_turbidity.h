#ifndef __TURBIDITY_H
#define __TURBIDITY_H

#include "main.h"

// 浊度传感器初始化
void Turbidity_Init(ADC_HandleTypeDef *hadc);

// 读取浊度电压值
float Turbidity_Read_Voltage(void);

// 读取浊度值 (NTU)
float Turbidity_Read_NTU(float water_temp);

#endif
