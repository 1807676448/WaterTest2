#include "app_turbidity.h"

// 警告：STM32 ADC引脚通常只能承受最高3.3V电压（VDD）。
// 如果浊度传感器输出最大4.5V，直接连接可能会损坏GPIO或导致读数在3.3V处限幅（截断）。
// 必须使用硬件分压电路（例如两个电阻串联）将0-4.5V映射到0-3.3V范围内。
// 这里定义分压系数：实际传感器电压 = ADC引脚电压 * 分压比
// 分压比 = (R上 + R下) / R下。 
// 例如：若要适配4.5V，建议将电压降至约3.0V。可用 10k(上) 和 20k(下)。
// 系数 = (10+20)/20 = 1.5。请根据实际电路修改此值。
#define TURBIDITY_DIVIDER_RATIO 1.0f 

static ADC_HandleTypeDef *turbidity_adc;

void Turbidity_Init(ADC_HandleTypeDef *hadc)
{
    turbidity_adc = hadc;
}

static void Turbidity_Channel_Config(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    sConfig.Channel = ADC_CHANNEL_1;  // 假设浊度传感器连接到ADC通道1 (PA1)
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    sConfig.OffsetSaturation = DISABLE;
    
    if (HAL_ADC_ConfigChannel(turbidity_adc, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

float Turbidity_Read_Voltage(void)
{
    uint32_t adc_val = 0;
    
    Turbidity_Channel_Config();
    
    HAL_ADC_Start(turbidity_adc);
    if (HAL_ADC_PollForConversion(turbidity_adc, 100) == HAL_OK)
    {
        adc_val = HAL_ADC_GetValue(turbidity_adc);
    }
    HAL_ADC_Stop(turbidity_adc);
    
    // 计算ADC引脚上的电压 (0 - 3.3V)
    float pin_voltage = (float)adc_val * 3.3f / 4095.0f;

    // 恢复传感器实际输出电压 (乘以分压系数)
    return pin_voltage * TURBIDITY_DIVIDER_RATIO;
}

float Turbidity_Read_NTU(void)
{
    float voltage = Turbidity_Read_Voltage();
    
    float ntu = voltage * (-865.68f) + 3291.3f;  

    return ntu;
}
