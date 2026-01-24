#include "app_debug.h"

static uint32_t SystemTick;
static uint8_t ZhuangTai;

void My_Usart_Send(char *str)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

void My_Usart_Send_Num(int num) {
    char buffer[20]; // 足够存储32位整数的字符串表示
    snprintf(buffer, sizeof(buffer), "%d", num);
    My_Usart_Send(buffer);
    My_Usart_Send("\n\r");
}

void My_Tick_Begin(void){
    SystemTick = HAL_GetTick();
}

uint32_t My_Tick_End(void){
    uint32_t a = HAL_GetTick()-SystemTick;
    My_Usart_Send_Num(a);
    My_Usart_Send("----------\n\r");
    return a;
}

uint32_t My_Tick_Get(void){
    uint32_t a = HAL_GetTick();
    if(ZhuangTai == 0){
        SystemTick = a;
        ZhuangTai = 1;
        return 0;
    }
    else{
        ZhuangTai = 0;
        My_Usart_Send_Num(a - SystemTick);
        return a - SystemTick;
    }
}

void HAL_Delay_Us(uint32_t us){
    __HAL_TIM_DISABLE(&htim7);
    __HAL_TIM_SET_COUNTER(&htim7, 0);
    __HAL_TIM_ENABLE(&htim7);
    while (__HAL_TIM_GET_COUNTER(&htim7) < us)
    {
    }
    __HAL_TIM_DISABLE(&htim7);

}

int fputc(int ch, FILE * f){
	HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xffff);///<普通串口发送数据
  	while(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET){}///<等待发送完成
  	return ch;
}


int fgetc(FILE * F) {
	uint8_t ch = 0;
	HAL_UART_Receive(&huart1,&ch, 1, 0xffff);///<普通串口接收数据
    while(__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET){}///<等待发送完成
	return ch; 
}
