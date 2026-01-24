#include "app_aht20_bmp280.h"

// ==================== 延迟函数 ====================
// 微秒级延迟函数（使用TIM7）
void AHT20_Delay_Us(uint32_t us)
{
    __HAL_TIM_DISABLE(&htim7);
    __HAL_TIM_SET_COUNTER(&htim7, 0);
    __HAL_TIM_ENABLE(&htim7);
    while (__HAL_TIM_GET_COUNTER(&htim7) < us)
    {
    }
    __HAL_TIM_DISABLE(&htim7);
}

// 毫秒级延迟函数
void AHT20_Delay_Ms(uint32_t ms)
{
    HAL_Delay(ms);
}

// ==================== I2C通信函数 ====================
// I2C写数据
HAL_StatusTypeDef AHT20_I2C_Write(I2C_HandleTypeDef *hi2c, uint16_t devAddr, uint8_t *pData, uint16_t size)
{
    return HAL_I2C_Master_Transmit(hi2c, devAddr, pData, size, I2C_TIMEOUT);
}

// I2C读数据
HAL_StatusTypeDef AHT20_I2C_Read(I2C_HandleTypeDef *hi2c, uint16_t devAddr, uint8_t regAddr, uint8_t *pData, uint16_t size)
{
    return HAL_I2C_Mem_Read(hi2c, devAddr, regAddr, I2C_MEMADD_SIZE_8BIT, pData, size, I2C_TIMEOUT);
}

// ==================== AHT20驱动部分 ====================
// 读取AHT20状态寄存器
uint8_t AHT20_Read_Status(I2C_HandleTypeDef *hi2c)
{
    uint8_t status = 0;
    uint8_t cmd = AHT20_STATUS_REG;

    // 发送读取状态寄存器的命令
    HAL_I2C_Master_Transmit(hi2c, AHT20_I2C_ADDRESS, &cmd, 1, I2C_TIMEOUT);

    // 读取状态寄存器
    HAL_I2C_Master_Receive(hi2c, AHT20_I2C_ADDRESS, &status, 1, I2C_TIMEOUT);

    return status;
}

// 检查AHT20校准是否完成
uint8_t AHT20_Read_Cal_Enable(I2C_HandleTypeDef *hi2c)
{
    uint8_t val = AHT20_Read_Status(hi2c);
    // Bit3: 校准使能, Bit7: 忙
    if ((val & 0x08) && ((val & 0x80) == 0x00))
        return 1;
    return 0;
}

// 初始化AHT20
uint8_t AHT20_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t cmd[3] = {AHT20_INIT_CMD, 0x08, 0x00};
    uint8_t retry_count = 0;

    // 软复位
    uint8_t reset_cmd = AHT20_SOFT_RESET;
    HAL_I2C_Master_Transmit(hi2c, AHT20_I2C_ADDRESS, &reset_cmd, 1, I2C_TIMEOUT);
    AHT20_Delay_Ms(20);

    // 发送初始化命令
    HAL_I2C_Master_Transmit(hi2c, AHT20_I2C_ADDRESS, cmd, 3, I2C_TIMEOUT);
    AHT20_Delay_Ms(10);

    // 等待校准完成
    while (AHT20_Read_Cal_Enable(hi2c) == 0)
    {
        AHT20_Delay_Ms(10);
        retry_count++;
        if (retry_count > 100) // 最大等待1秒
            return 0;
    }

    return 1;
}

// 读取AHT20温湿度数据
uint8_t AHT20_Read_CTdata(I2C_HandleTypeDef *hi2c, uint32_t *ct)
{
    uint8_t cmd[3] = {AHT20_START_MEASURE, 0x33, 0x00};
    uint8_t data[6] = {0};
    uint8_t retry = 0;

    // 1. 发送开始测量命令（带重试）
    HAL_StatusTypeDef hal_status;
    while (retry < 3)
    {
        hal_status = HAL_I2C_Master_Transmit(hi2c, AHT20_I2C_ADDRESS, cmd, 3, I2C_TIMEOUT);
        if (hal_status == HAL_OK)
            break;
        retry++;
        AHT20_Delay_Ms(10);
    }

    if (hal_status != HAL_OK)
    {
        printf("AHT20发送命令失败（已重试%d次）: %d\r\n", retry, hal_status);
        return 0;
    }

    // 2. 等待测量完成 (AHT20典型测量时间为80ms)
    AHT20_Delay_Ms(80);

    // 3. 读取数据 (状态字 + 湿度 + 温度)
    hal_status = HAL_I2C_Master_Receive(hi2c, AHT20_I2C_ADDRESS, data, 6, I2C_TIMEOUT);
    if (hal_status != HAL_OK)
    {
        printf("AHT20读取数据失败: %d\r\n", hal_status);
        return 0;
    }

    // 4. 检查状态位 (Bit7: 忙, Bit3: 校准)
    if ((data[0] & 0x80) != 0)
    {
        printf("AHT20仍在忙\r\n");
        return 0;
    }

    // 6. 验证数据校验和（可选）
    // AHT20不提供校验和，但可以检查数据是否合理

    // 7. 解析数据
    // ct[0] = ((0|((uint32_t)data[1] << 8))|(uint32_t)data[2]) << 8) | (uint32_t)data[3] >> 4;
    // ct[1] = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];

    uint32_t RetuData;

    RetuData = 0;
    RetuData = (RetuData | data[1]) << 8;
    RetuData = (RetuData | data[2]) << 8;
    RetuData = (RetuData | data[3]);
    RetuData = RetuData >> 4;
    ct[0] = RetuData;

    RetuData = 0;
    RetuData = (RetuData | (data[3] & 0x0F)) << 8;
    RetuData = (RetuData | data[4]) << 8;
    RetuData = (RetuData | data[5]);
    RetuData = RetuData & 0xfffff;
    ct[1] = RetuData;

    return 1;
}

// ==================== BMP280驱动部分 ====================
static bmp280Calib bmp280_calib;
static int32_t bmp280_raw_pressure = 0;
static int32_t bmp280_raw_temperature = 0;

// BMP280配置
#define BMP280_PRESSURE_OSR BMP280_OVERSAMP_8X
#define BMP280_TEMPERATURE_OSR BMP280_OVERSAMP_16X
#define BMP280_MODE (BMP280_PRESSURE_OSR << 2 | BMP280_TEMPERATURE_OSR << 5 | BMP280_NORMAL_MODE)

// 初始化BMP280
uint8_t BMP280_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t chip_id = 0;
    uint8_t config_data[2];

    // 读取芯片ID
    HAL_I2C_Mem_Read(hi2c, BMP280_I2C_ADDRESS, BMP280_CHIPID_REG,
                     I2C_MEMADD_SIZE_8BIT, &chip_id, 1, I2C_TIMEOUT);

    // 读取校准数据
    HAL_I2C_Mem_Read(hi2c, BMP280_I2C_ADDRESS, BMP280_DIG_T1_LSB_REG,
                     I2C_MEMADD_SIZE_8BIT, (uint8_t *)&bmp280_calib, 24, I2C_TIMEOUT);

    // 配置测量模式
    config_data[0] = BMP280_MODE;
    HAL_I2C_Mem_Write(hi2c, BMP280_I2C_ADDRESS, BMP280_CTRLMEAS_REG,
                      I2C_MEMADD_SIZE_8BIT, config_data, 1, I2C_TIMEOUT);

    // 配置滤波器（IIR滤波器系数5）
    config_data[0] = (5 << 2);
    HAL_I2C_Mem_Write(hi2c, BMP280_I2C_ADDRESS, BMP280_CONFIG_REG,
                      I2C_MEMADD_SIZE_8BIT, config_data, 1, I2C_TIMEOUT);

    return chip_id; // BMP280芯片ID应为0x58
}

// 读取原始压力数据
static HAL_StatusTypeDef BMP280_Read_Raw_Data(I2C_HandleTypeDef *hi2c)
{
    uint8_t data[6];
    HAL_StatusTypeDef status;

    // 读取原始数据
    status = HAL_I2C_Mem_Read(hi2c, BMP280_I2C_ADDRESS, BMP280_PRESSURE_MSB_REG,
                              I2C_MEMADD_SIZE_8BIT, data, 6, I2C_TIMEOUT);

    if (status != HAL_OK)
    {
        printf("BMP280读取失败: %d\r\n", status);
        return status;
    }

    // 检查数据是否有效（读取的值是否为0）
    uint8_t all_zero = 1;
    for (int i = 0; i < 6; i++)
    {
        if (data[i] != 0)
        {
            all_zero = 0;
            break;
        }
    }

    if (all_zero)
    {
        printf("BMP280读取到全零数据\r\n");
        return HAL_ERROR;
    }

    // 组合压力数据（20位）
    bmp280_raw_pressure = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | ((int32_t)data[2] >> 4);

    // 组合温度数据（20位）
    bmp280_raw_temperature = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | ((int32_t)data[5] >> 4);

    return HAL_OK;
}

// 温度补偿计算
static int32_t BMP280_Compensate_Temperature(int32_t adc_t)
{
    int32_t var1, var2, t;

    var1 = ((((adc_t >> 3) - ((int32_t)bmp280_calib.dig_T1 << 1))) *
            ((int32_t)bmp280_calib.dig_T2)) >>
           11;

    var2 = (((((adc_t >> 4) - ((int32_t)bmp280_calib.dig_T1)) *
              ((adc_t >> 4) - ((int32_t)bmp280_calib.dig_T1))) >>
             12) *
            ((int32_t)bmp280_calib.dig_T3)) >>
           14;

    bmp280_calib.t_fine = var1 + var2;
    t = (bmp280_calib.t_fine * 5 + 128) >> 8;

    return t;
}

// 压力补偿计算
static uint32_t BMP280_Compensate_Pressure(int32_t adc_p)
{
    int64_t var1, var2, p;

    var1 = ((int64_t)bmp280_calib.t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)bmp280_calib.dig_P6;
    var2 = var2 + ((var1 * (int64_t)bmp280_calib.dig_P5) << 17);
    var2 = var2 + (((int64_t)bmp280_calib.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)bmp280_calib.dig_P3) >> 8) +
           ((var1 * (int64_t)bmp280_calib.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)bmp280_calib.dig_P1) >> 33;

    if (var1 == 0)
        return 0;

    p = 1048576 - adc_p;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)bmp280_calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)bmp280_calib.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)bmp280_calib.dig_P7) << 4);

    // 合理性检查：气压范围 300-1100 hPa (7680000 - 28160000, 单位: Pa*256)
    if (p < 7680000 || p > 28160000)
    {
        printf("BMP280气压值超出合理范围: %ld\r\n", (long)p);
        return 0; // 返回无效值
    }

    return (uint32_t)p;
}

// 压力转海拔高度
#define CONST_PF 0.1902630958 // (1/5.25588f)
#define FIX_TEMP 25           // 固定温度（标准温度）
static float BMP280_Pressure_To_Altitude(float *pressure)
{
    if (*pressure > 0)
    {
        return ((pow((1015.7f / *pressure), CONST_PF) - 1.0f) *
                (FIX_TEMP + 273.15f)) /
               0.0065f;
    }
    else
    {
        return 0;
    }
}

// 限幅平均滤波
#define FILTER_NUM 5
#define FILTER_A 0.1f
static void Pressure_Filter(float *in, float *out)
{
    static uint8_t i = 0;
    static float filter_buf[FILTER_NUM] = {0.0};
    float filter_sum = 0.0;
    uint8_t cnt = 0;
    float delta;

    if (filter_buf[i] == 0.0f)
    {
        filter_buf[i] = *in;
        *out = *in;
        if (++i >= FILTER_NUM)
            i = 0;
    }
    else
    {
        if (i)
            delta = *in - filter_buf[i - 1];
        else
            delta = *in - filter_buf[FILTER_NUM - 1];

        if (fabs(delta) < FILTER_A)
        {
            filter_buf[i] = *in;
            if (++i >= FILTER_NUM)
                i = 0;
        }

        for (cnt = 0; cnt < FILTER_NUM; cnt++)
        {
            filter_sum += filter_buf[cnt];
        }
        *out = filter_sum / FILTER_NUM;
    }
}

// 修改后的 BMP280GetData 函数
void BMP280GetData(I2C_HandleTypeDef *hi2c, float *pressure, float *temperature, float *altitude)
{
    static float temp_filtered, press_filtered;
    static uint32_t last_read_time = 0;
    uint32_t current_time = HAL_GetTick();

    // 限制读取频率（至少间隔100ms）
    if ((current_time - last_read_time) < 100 && last_read_time != 0)
    {
        // 使用上一次的数据
        return;
    }

    last_read_time = current_time;

    // 读取原始数据
    if (BMP280_Read_Raw_Data(hi2c) != HAL_OK)
    {
        // 读取失败，使用上一次的有效数据
        return;
    }

    // 计算温度和压力
    int32_t temp_raw = BMP280_Compensate_Temperature(bmp280_raw_temperature);
    temp_filtered = temp_raw / 100.0f;

    uint32_t press_raw = BMP280_Compensate_Pressure(bmp280_raw_pressure);
    
    // 检查压力值是否有效
    if (press_raw == 0)
    {
        // 使用上次有效数据
        return;
    }
    
    press_filtered = press_raw / 25600.0f;

    // 使用限幅滤波器进行滤波处理
    Pressure_Filter(&press_filtered, pressure);

    *temperature = temp_filtered;
    *altitude = BMP280_Pressure_To_Altitude(pressure);
}
