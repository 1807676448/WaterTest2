#ifndef  MY_DEBUG_H
#define  MY_DEBUG_H

#include "main.h"
#include <stdio.h>
#include <string.h>

void My_Usart_Send(char *str);
void My_Usart_Send_Num(int num);
void My_Tick_Begin(void);
uint32_t My_Tick_End(void);
uint32_t My_Tick_Get(void);
void HAL_Delay_Us(uint32_t us);


#endif 