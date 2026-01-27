#ifndef __DS18B20_H
#define __DS18B20_H

#include "main.h"
#include <stdint.h>

#define HIGH  1
#define LOW   0
 
#define DS18B20_CLK     RCC_APB2Periph_GPIOB
#define DS18B20_PIN     GPIO_PIN_10                  
#define DS18B20_PORT		GPIOC 
 
//带参宏，可以像内联函数一样使用,输出高电平或低电平
#define DS18B20_DATA_OUT(a)	HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, a)
 //读取引脚的电平
#define  DS18B20_DATA_IN()	   HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN)
 
typedef struct
{
	uint8_t  humi_int;		//湿度的整数部分
	uint8_t  humi_deci;	 	//湿度的小数部分
	uint8_t  temp_int;	 	//温度的整数部分
	uint8_t  temp_deci;	 	//温度的小数部分
	uint8_t  check_sum;	 	//校验和
		                 
}DS18B20_Data_TypeDef;
 
uint8_t DS18B20_Init(void);
float DS18B20_Get_Temp(void);
#endif /* __DS18B20_H */
