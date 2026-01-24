#include "app_ds18b20.h"

/**************************************************************************************
 * 描  述 : 仅用于初始化：配置使DS18B20-DATA引脚变为开漏输出模式
 * 入  参 : 无
 * 返回值 : 无
 **************************************************************************************/
static void DS18B20_Mode_Out_OD(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	GPIO_InitStruct.Pin = DS18B20_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD; // 开漏输出，需外部/内部上拉
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(DS18B20_PORT, &GPIO_InitStruct);
}

/**************************************************************************************
 * 描  述 : 主机给从机发送复位脉冲
 * 入  参 : 无
 * 返回值 : 无
 **************************************************************************************/
static void DS18B20_Rst(void)
{
    // 复位信号：拉低至少480us
	DS18B20_DATA_OUT(GPIO_PIN_RESET); 
	HAL_Delay_Us(750);
    
    // 释放总线（写入1，由于是OD模式，会变成高阻/上拉）
	DS18B20_DATA_OUT(GPIO_PIN_SET); 
	
    // 等待从机响应（15-60us后拉低）
	HAL_Delay_Us(15); 
}

/**************************************************************************************
 * 描  述 : 检测从机给主机返回的存在脉冲
 * 入  参 : 无
 * 返回值 : 0：成功   1：失败
 **************************************************************************************/
static uint8_t DS18B20_Presence(void)
{
	uint8_t pulse_time = 0;

    // 此时总线已被Rst函数释放

	/* 等待存在脉冲的到来，存在脉冲为一个60~240us的低电平信号
	 * 如果存在脉冲没有来则做超时处理，从机接收到主机的复位信号后，会在15~60us后给主机发一个存在脉冲
	 */
	while (DS18B20_DATA_IN() && pulse_time < 100)
	{
		pulse_time++;
		HAL_Delay_Us(1);
	}

	if (pulse_time >= 100) // 经过100us后，存在脉冲都还没有到来
		return 1;		   // 读取失败
	else				   // 经过100us后，存在脉冲到来
		pulse_time = 0;	   // 清零计时变量

	while (!DS18B20_DATA_IN() && pulse_time < 240) // 存在脉冲到来，且存在的时间不能超过240us
	{
		pulse_time++;
		HAL_Delay_Us(1);
	}
	if (pulse_time >= 240) // 存在脉冲到来，且存在的时间超过了240us
		return 1;		   // 读取失败
	else
		return 0;
}

/**************************************************************************************
 * 描  述 : 从DS18B20读取一个bit
 * 入  参 : 无
 * 返回值 : u8
 **************************************************************************************/
static uint8_t DS18B20_Read_Bit(void)
{
	uint8_t dat;

    /* Start Slot: 拉低 >1us (这里2us) */
	DS18B20_DATA_OUT(GPIO_PIN_RESET);
	HAL_Delay_Us(2);

	/* Release: 释放总线(OD模式写1)，准备采样 */
	DS18B20_DATA_OUT(GPIO_PIN_SET);
    
    /* 等待数据稳定：采样点应在时隙开始后15us内
       前面延时2us，这里再延时8-10us，总共~10-12us采样，满足时序 */
    HAL_Delay_Us(8);

    /* 采样 */
	if (DS18B20_DATA_IN() == GPIO_PIN_SET)
		dat = 1;
	else
		dat = 0;

	/* 补齐时隙：读时隙至少60us */
	HAL_Delay_Us(50);

	return dat;
}

/**************************************************************************************
 * 描  述 : 从DS18B20读一个字节，低位先行
 * 入  参 : 无
 * 返回值 : u8
 **************************************************************************************/
uint8_t DS18B20_Read_Byte(void)
{
	uint8_t i, j, dat = 0;

	for (i = 0; i < 8; i++)
	{
		j = DS18B20_Read_Bit(); // 从DS18B20读取一个bit
		dat = (dat) | (j << i);
	}

	return dat;
}

/**************************************************************************************
 * 描  述 : 写一个字节到DS18B20，低位先行
 * 入  参 : u8
 * 返回值 : 无
 **************************************************************************************/
void DS18B20_Write_Byte(uint8_t dat)
{
	uint8_t i, testb;

	for (i = 0; i < 8; i++)
	{
		testb = dat & 0x01;
		dat = dat >> 1;
		
		if (testb) // 写1
		{
            /* 拉低 >1us (起始) */
			DS18B20_DATA_OUT(GPIO_PIN_RESET);
			HAL_Delay_Us(2); 

            /* 释放 (写1) */
			DS18B20_DATA_OUT(GPIO_PIN_SET);
            
            /* 等待时隙结束 (至少60us) */
			HAL_Delay_Us(60); 
		}
		else // 写0
		{
            /* 拉低 (起始) */
			DS18B20_DATA_OUT(GPIO_PIN_RESET);
            
            /* 保持低电平 60us-120us (写0) */
			HAL_Delay_Us(60);

            /* 释放 */
			DS18B20_DATA_OUT(GPIO_PIN_SET);
            
            /* 恢复时间 >1us */
			HAL_Delay_Us(2);
		}
	}
}

/**************************************************************************************
 * 描  述 : 起始DS18B20
 * 入  参 : 无
 * 返回值 : 无
 **************************************************************************************/
void DS18B20_Start(void)
{
	DS18B20_Rst();			  // 主机给从机发送复位脉冲
	if (DS18B20_Presence()) return; // 检测失败则退出
	DS18B20_Write_Byte(0XCC); // 跳过 ROM
	DS18B20_Write_Byte(0X44); // 开始转换
}

/**************************************************************************************
 * 描  述 : DS18B20初始化函数
 * 入  参 : 无
 * 返回值 : u8
 **************************************************************************************/
uint8_t DS18B20_Init(void)
{
    // 仅在此处配置一次GPIO为开漏模式
	DS18B20_Mode_Out_OD();
    
	DS18B20_Rst();
	return DS18B20_Presence();
}

/**************************************************************************************
 * 描  述 : 从DS18B20读取温度值
 * 入  参 : 无
 * 返回值 : float
 **************************************************************************************/
float DS18B20_Get_Temp(void)
{
	uint8_t tpmsb, tplsb;
	short s_tem;
	float f_tem;

	DS18B20_Rst();
	if (DS18B20_Presence())
	{
		return 0.0f; // 未检测到从机，直接返回0以便上层判定
	}
	DS18B20_Write_Byte(0XCC); /* 跳过 ROM */
	DS18B20_Write_Byte(0X44); /* 开始转换 */

	/* 等待转换完成 (12位精度需要750ms) */
	HAL_Delay(800);

	DS18B20_Rst();
	if (DS18B20_Presence())
	{
		return 0.0f;
	}
	DS18B20_Write_Byte(0XCC); /* 跳过 ROM */
	DS18B20_Write_Byte(0XBE); /* 读温度值 */

	tplsb = DS18B20_Read_Byte();
	tpmsb = DS18B20_Read_Byte();

	s_tem = tpmsb << 8;
	s_tem = s_tem | tplsb;

	if (s_tem < 0) /* 负温度 */
		f_tem = (~s_tem + 1) * 0.0625;
	else
		f_tem = (s_tem * 0.0625);

	// 这样做的目的将小数点后第一位也转换为可显示数字
	// 同时进行一个四舍五入操作。

	// printf("DS18B20 Tem is %.6f\n\r", f_tem);
	return f_tem;
}

/*************************************END OF FILE******************************/
