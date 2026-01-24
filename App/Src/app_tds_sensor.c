#include "app_tds_sensor.h"

// 存储ADC句柄
static ADC_HandleTypeDef *p_hadc1 = NULL;

// 校准系数，默认为1.0
static float kValue = 1.0f;

// 存储最后读取的数据
static TDS_Data_TypeDef tds_data = {0};

/***
 * @brief TDS通道配置函数
 * @return 无
 */
static void TDS_Channel_Config(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    // 配置ADC1通道3 (PA2)
    sConfig.Channel = ADC_CHANNEL_3;              // ADC1_INP3 对应PA2
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;  // 与pH采样时间一致
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    sConfig.OffsetSaturation = DISABLE;

    if (HAL_ADC_ConfigChannel(p_hadc1, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

/***
 * @brief 标准TDS曲线计算函数
 * 根据附件标准曲线: y = 66.71x^3 - 127.93x^2 + 428.7x
 * @param voltage: 输入电压值(V)
 * @return 返回对应的NTU值(ppm)
 */
static float TDS_Curve_Calculate(float voltage)
{
    // 标准曲线系数
    float a = 66.71f;
    float b = -127.93f;
    float c = 428.7f;

    // y = ax^3 + bx^2 + cx
    float tds_value = a * voltage * voltage * voltage + b * voltage * voltage + c * voltage;

    // 限制范围在0-1400ppm
    if (tds_value < 0.0f)
        tds_value = 0.0f;
    if (tds_value > 1400.0f)
        tds_value = 1400.0f;

    return tds_value;
}

/***
 * @brief 温度校正系数计算
 * 根据附件校正公式: T修正 = 1 + 0.02*(T - 25)
 * @param temperature: 当前温度(°C)
 * @return 返回温度校正系数
 */
static float TDS_Temperature_Correction_Factor(float temperature)
{
    // T_corrected = 1 + 0.02 * (T - 25)
    float t_factor = 1.0f + 0.02f * (temperature - 25.0f);
    return t_factor;
}

/***
 * @brief TDS初始化函数
 * @param hadc: ADC句柄指针
 * @return 返回0表示成功，非0表示失败
 */
uint8_t TDS_Init(ADC_HandleTypeDef *hadc)
{
    if (hadc == NULL)
        return 1;

    p_hadc1 = hadc;

    // 初始化DS18B20温度传感器
    if (DS18B20_Init() != 0)
    {
        printf("DS18B20 Init Failed!\n\r");
        return 1;
    }

    // 配置ADC通道
    TDS_Channel_Config();

    // 默认K值为1.0
    kValue = 1.0f;

    printf("TDS Sensor Init Success!\n\r");
    return 0;
}

/***
 * @brief 读取原始TDS值(未温度校正)
 * @return 返回TDS值(ppm)
 */
float TDS_Read_Raw(void)
{
    uint32_t adc_value;
    float voltage;
    float tds_value;

    if (p_hadc1 == NULL)
        return 0.0f;

    // 重新配置ADC通道
    TDS_Channel_Config();

    // 开始ADC转换
    HAL_ADC_Start(p_hadc1);
    if (HAL_ADC_PollForConversion(p_hadc1, 10) == HAL_OK)
    {
        adc_value = HAL_ADC_GetValue(p_hadc1);
    }
    else
    {
        HAL_ADC_Stop(p_hadc1);
        return tds_data.raw_tds;  // 返回上次数据
    }
    HAL_ADC_Stop(p_hadc1);

    // 转换为电压值 (0-3.3V, 12bit精度)
    voltage = (adc_value / 4095.0f) * 3.3f;

    // 使用标准曲线计算TDS值
    tds_value = TDS_Curve_Calculate(voltage);

    // 应用K值校准
    tds_value = tds_value * kValue;

    // 保存原始值
    tds_data.raw_voltage = voltage;
    tds_data.raw_tds = tds_value;

    return tds_value;
}

/***
 * @brief 读取温度校正后的TDS值
 * @return 返回校正后的TDS值(ppm)
 */
float TDS_Read_Corrected(void)
{
    float temperature;
    float t_correction_factor;
    float tds_corrected;
    float v_corrected;

    // 1. 读取原始数据以获取最新电压 (更新 tds_data.raw_voltage)
    TDS_Read_Raw();

    // 2. 读取DS18B20温度
    temperature = DS18B20_Get_Temp();
    tds_data.temperature = temperature;

    // 3. 计算温度校正系数
    // T_factor = 1 + 0.02 * (T - 25)
    t_correction_factor = TDS_Temperature_Correction_Factor(temperature);

    // 4. 应用温度校正到电压 (标准做法)
    // 根据物理特性，温度升高电导率升高，电压升高。
    // 要归一化到25℃，需除以校正系数。
    // 例如：35℃时系数1.2，测得1.2V，归一化后为1.0V(即25℃时的等效电压)
    if (t_correction_factor != 0.0f)
    {
        v_corrected = tds_data.raw_voltage / t_correction_factor;
    }
    else
    {
        v_corrected = tds_data.raw_voltage;
    }

    // 5. 使用校正后的电压重新计算TDS
    tds_corrected = TDS_Curve_Calculate(v_corrected);

    // 6. 应用K值校准
    tds_corrected = tds_corrected * kValue;

    // 限制范围
    if (tds_corrected < 0.0f)
        tds_corrected = 0.0f;
    if (tds_corrected > 1400.0f)
        tds_corrected = 1400.0f;

    tds_data.tds_corrected = tds_corrected;

    return tds_corrected;
}

/***
 * @brief 获取完整的TDS数据结构
 * @return 返回包含所有数据的TDS_Data_TypeDef结构体
 */
TDS_Data_TypeDef TDS_Get_Data(void)
{
    // 更新校正后的值
    TDS_Read_Corrected();
    tds_data.k_value = kValue;
    return tds_data;
}

/***
 * @brief 设置TDS校准系数
 * 校准方法:
 * 1. 使用标准TDS溶液测量(例如90ppm标准液)
 * 2. 记录测量值 TDS_measured
 * 3. K = TDS_standard / TDS_measured
 * 4. 调用此函数设置K值
 * @param k_value: 校准系数
 */
void TDS_Set_K_Value(float k_value)
{
    if (k_value > 0.0f && k_value < 10.0f)  // 限制K值范围
    {
        kValue = k_value;
        printf("TDS K Value Set to: %.2f\n\r", kValue);
    }
    else
    {
        printf("Invalid K Value! Range: 0-10\n\r");
    }
}

/***
 * @brief 获取当前的K值
 * @return 返回当前的校准系数
 */
float TDS_Get_K_Value(void)
{
    return kValue;
}

/*************************************END OF FILE******************************/
