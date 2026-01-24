#ifndef __AHT20_BMP280_H
#define __AHT20_BMP280_H

#include "main.h"
#include <math.h>

// ==================== I2C配置部分 ====================
// 使用硬件I2C1，引脚配置在CubeMX中完成
#ifndef I2C_HANDLE
#define I2C_HANDLE    hi2c1  // I2C句柄，需要在main.c中定义
#endif

// I2C超时时间（毫秒）
#ifndef I2C_TIMEOUT
#define I2C_TIMEOUT   100
#endif

// ==================== AHT20相关定义 ====================
#define AHT20_I2C_ADDRESS    0x38 << 1    // I2C从机地址（左移1位，HAL库要求）

// AHT20内部地址
#define AHT20_INIT_CMD       0xBE    // 初始化
#define AHT20_SOFT_RESET     0xBA    // 软复位
#define AHT20_START_MEASURE  0xAC    // 开始测量
#define AHT20_STATUS_REG     0x00    // 状态寄存器

// ==================== BMP280相关定义 ====================
#define BMP280_I2C_ADDRESS   0x77 << 1    // I2C从机地址（左移1位，HAL库要求）

// BMP280校准参数寄存器
#define BMP280_DIG_T1_LSB_REG                0x88
#define BMP280_DIG_T1_MSB_REG                0x89
#define BMP280_DIG_T2_LSB_REG                0x8A
#define BMP280_DIG_T2_MSB_REG                0x8B
#define BMP280_DIG_T3_LSB_REG                0x8C
#define BMP280_DIG_T3_MSB_REG                0x8D
#define BMP280_DIG_P1_LSB_REG                0x8E
#define BMP280_DIG_P1_MSB_REG                0x8F
#define BMP280_DIG_P2_LSB_REG                0x90
#define BMP280_DIG_P2_MSB_REG                0x91
#define BMP280_DIG_P3_LSB_REG                0x92
#define BMP280_DIG_P3_MSB_REG                0x93
#define BMP280_DIG_P4_LSB_REG                0x94
#define BMP280_DIG_P4_MSB_REG                0x95
#define BMP280_DIG_P5_LSB_REG                0x96
#define BMP280_DIG_P5_MSB_REG                0x97
#define BMP280_DIG_P6_LSB_REG                0x98
#define BMP280_DIG_P6_MSB_REG                0x99
#define BMP280_DIG_P7_LSB_REG                0x9A
#define BMP280_DIG_P7_MSB_REG                0x9B
#define BMP280_DIG_P8_LSB_REG                0x9C
#define BMP280_DIG_P8_MSB_REG                0x9D
#define BMP280_DIG_P9_LSB_REG                0x9E
#define BMP280_DIG_P9_MSB_REG                0x9F

// BMP280控制寄存器
#define BMP280_CHIPID_REG                    0xD0  // Chip ID Register
#define BMP280_RESET_REG                     0xE0  // Softreset Register
#define BMP280_STATUS_REG                    0xF3  // Status Register
#define BMP280_CTRLMEAS_REG                  0xF4  // Ctrl Measure Register
#define BMP280_CONFIG_REG                    0xF5  // Configuration Register
#define BMP280_PRESSURE_MSB_REG              0xF7  // Pressure MSB Register
#define BMP280_PRESSURE_LSB_REG              0xF8  // Pressure LSB Register
#define BMP280_PRESSURE_XLSB_REG             0xF9  // Pressure XLSB Register
#define BMP280_TEMPERATURE_MSB_REG           0xFA  // Temperature MSB Reg
#define BMP280_TEMPERATURE_LSB_REG           0xFB  // Temperature LSB Reg
#define BMP280_TEMPERATURE_XLSB_REG          0xFC  // Temperature XLSB Reg

// BMP280工作模式
#define BMP280_SLEEP_MODE                    (0x00)
#define BMP280_FORCED_MODE                   (0x01)
#define BMP280_NORMAL_MODE                   (0x03)

// 采样率设置
#define BMP280_OVERSAMP_SKIPPED              (0x00)
#define BMP280_OVERSAMP_1X                   (0x01)
#define BMP280_OVERSAMP_2X                   (0x02)
#define BMP280_OVERSAMP_4X                   (0x03)
#define BMP280_OVERSAMP_8X                   (0x04)
#define BMP280_OVERSAMP_16X                  (0x05)

// 数据帧大小
#define BMP280_DATA_FRAME_SIZE               (6)

// BMP280校准数据结构
typedef struct
{
    uint16_t dig_T1;  // 校准T1数据
    int16_t  dig_T2;  // 校准T2数据
    int16_t  dig_T3;  // 校准T3数据
    uint16_t dig_P1;  // 校准P1数据
    int16_t  dig_P2;  // 校准P2数据
    int16_t  dig_P3;  // 校准P3数据
    int16_t  dig_P4;  // 校准P4数据
    int16_t  dig_P5;  // 校准P5数据
    int16_t  dig_P6;  // 校准P6数据
    int16_t  dig_P7;  // 校准P7数据
    int16_t  dig_P8;  // 校准P8数据
    int16_t  dig_P9;  // 校准P9数据
    int32_t  t_fine;  // 校准t_fine数据
} bmp280Calib;

// ==================== 函数声明 ====================
// AHT20函数
uint8_t AHT20_Init(I2C_HandleTypeDef *hi2c);
uint8_t AHT20_Read_Cal_Enable(I2C_HandleTypeDef *hi2c);
uint8_t AHT20_Read_CTdata(I2C_HandleTypeDef *hi2c, uint32_t *ct);  // 修改为返回uint8_t
uint8_t AHT20_Read_Status(I2C_HandleTypeDef *hi2c);

// BMP280函数
uint8_t BMP280_Init(I2C_HandleTypeDef *hi2c);
void BMP280GetData(I2C_HandleTypeDef *hi2c, float* pressure, float* temperature, float* asl);

// 工具函数
void AHT20_Delay_Us(uint32_t us);
void AHT20_Delay_Ms(uint32_t ms);
HAL_StatusTypeDef AHT20_I2C_Write(I2C_HandleTypeDef *hi2c, uint16_t devAddr, uint8_t *pData, uint16_t size);
HAL_StatusTypeDef AHT20_I2C_Read(I2C_HandleTypeDef *hi2c, uint16_t devAddr, uint8_t regAddr, uint8_t *pData, uint16_t size);

#endif /* __AHT20_BMP280_H */
