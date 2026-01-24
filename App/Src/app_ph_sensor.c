#include "app_ph_sensor.h"

float PH_Value;

static void PH_Channel_Config(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    sConfig.Channel = ADC_CHANNEL_2;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    sConfig.OffsetSaturation = DISABLE;
    
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

float PH_Read_Median(void)
{
    #define SAMPLE_COUNT 7  // 奇数个样本，便于取中值
    uint32_t samples[SAMPLE_COUNT];
    
    // 配置通道
    PH_Channel_Config();

    // 采集样本
    for(int i = 0; i < SAMPLE_COUNT; i++)
    {
        HAL_ADC_Start(&hadc1);
        if(HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
        {
            samples[i] = HAL_ADC_GetValue(&hadc1);
        }
        HAL_ADC_Stop(&hadc1);
        HAL_Delay(1);
    }
    
    // 排序样本（冒泡排序）
    for(int i = 0; i < SAMPLE_COUNT-1; i++)
    {
        for(int j = 0; j < SAMPLE_COUNT-i-1; j++)
        {
            if(samples[j] > samples[j+1])
            {
                uint32_t temp = samples[j];
                samples[j] = samples[j+1];
                samples[j+1] = temp;
            }
        }
    }
    
    // 取中值
    uint32_t median_adc = samples[SAMPLE_COUNT/2];
    
    // 转换为电压
    float voltage = (median_adc / 4095.0f) * 3.3f;
    
    // 计算pH值
    float PH_Value = (-5.7541 * voltage + 16.654);
    
    // 限制范围
    if(PH_Value < 0.0f) PH_Value = 0.0f;
    if(PH_Value > 14.0f) PH_Value = 14.0f;
    
    return PH_Value;
}