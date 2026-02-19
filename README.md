# WaterTest2 水质监测系统（STM32G431）

基于 **STM32G431** 的多传感器水质监测固件工程，采集水质与环境参数，并通过串口输出调试信息与 JSON 数据帧，适用于上位机实时显示、数据记录与物联网网关接入。

---

## 1. 功能概览

系统周期性采集以下数据：

- 水质相关：
  - 水温（DS18B20）
  - pH（模拟量 ADC）
  - TDS（模拟量 ADC，含温度补偿）
  - 浊度（模拟量 ADC，含温度补偿）
- 环境相关：
  - 空气温湿度（AHT20，I2C）
  - 气压与海拔（BMP280，I2C）

采集周期由 `report_interval` 控制，当前默认值为 **1500 ms**。

系统每个周期执行：

1. 读取所有传感器数据
2. 通过 `USART1` 打印人类可读调试信息（`printf`）
3. 通过 `USART2` 发送 JSON 数据帧给上位机

---

## 2. 工程结构

```text
WaterTest2/
├─ App/
│  ├─ Inc/                # 业务模块头文件
│  └─ Src/                # 业务模块实现（传感器、通信、调试）
├─ Core/
│  ├─ Inc/                # HAL/Cube 头文件与主头
│  └─ Src/                # main、外设初始化、中断等
├─ Drivers/               # STM32 HAL 与 CMSIS
├─ MDK-ARM/               # Keil 工程与构建输出
└─ WaterTest2.ioc         # CubeMX 工程配置
```

主要业务模块说明：

- `app_ph_sensor.*`：pH 采样与中值滤波
- `app_tds_sensor.*`：TDS 电导率换算、温度补偿、K 值校准
- `app_turbidity.*`：浊度电压读取与 NTU 计算
- `app_ds18b20.*`：1-Wire 温度采集
- `app_aht20_bmp280.*`：I2C 温湿度/气压/海拔采集
- `app_json_comm.*`：串口 JSON 上报
- `app_debug.*`：`printf` 重定向、微秒延时

---

## 3. 硬件与引脚映射（按当前代码）

## 3.1 MCU 与基础外设

- MCU：STM32G431
- 系统时钟：HSI + PLL（`SystemClock_Config`）
- ADC：ADC1，12-bit
- I2C：I2C2
- UART：USART1 / USART2 / USART3（115200, 8N1）

## 3.2 传感器与接口映射

### 模拟量传感器（ADC1）

- `PA0 -> ADC1_IN1`：浊度（`app_turbidity.c`）
- `PA1 -> ADC1_IN2`：pH（`app_ph_sensor.c`）
- `PA2 -> ADC1_IN3`：TDS（`app_tds_sensor.c`）

### I2C 传感器（I2C2）

- `PA8 -> I2C2_SDA`
- `PA9 -> I2C2_SCL`
- AHT20 地址：`0x38 << 1`
- BMP280 地址：`0x77 << 1`

### DS18B20（1-Wire）

- `PC10`：DS18B20 数据线（开漏输出模式）

### 串口

- USART1（调试输出）
  - `PC4 -> TX`
  - `PC5 -> RX`
- USART2（上位机通信/JSON）
  - `PB3 -> TX`
  - `PA3 -> RX`
- USART3（预留）
  - `PB10 -> TX`
  - `PB11 -> RX`

---

## 4. 软件执行流程

上电后主要流程（`Core/Src/main.c`）：

1. HAL 与系统时钟初始化
2. GPIO/ADC/I2C/TIM/UART 初始化
3. 依次初始化传感器：TDS、浊度、DS18B20、AHT20、BMP280
4. 初始化通信接收（USART2 中断）
5. 进入主循环，按 `report_interval` 周期采集并发送数据

主循环每次采样包含：

- `PH_Read_Median()`
- `TDS_Read_Corrected()`（内部读取 DS18B20 温度补偿）
- `DS18B20_Get_Temp()`
- `Turbidity_Read_NTU(water_temp)`
- `AHT20_Read_CTdata(...)`
- `BMP280GetData(...)`
- `Comm_Send_Sensor_Data(...)`

---

## 5. 数据模型与算法说明

## 5.1 pH（`app_ph_sensor.c`）

- 采样数：7 次（奇数样本）
- 滤波：冒泡排序后取中值
- 电压换算：

  \[
  V = \frac{ADC}{4095} \times 3.3
  \]

- pH 拟合：

  \[
  pH = -5.7541 \times V + 16.654
  \]

- 限幅：`0.0 ~ 14.0`

## 5.2 TDS（`app_tds_sensor.c`）

- 原始曲线：

  \[
  TDS = 66.71V^3 - 127.93V^2 + 428.7V
  \]

- 温度补偿系数：

  \[
  k_T = 1 + 0.02(T-25)
  \]

- 25℃ 等效电压：

  \[
  V_{25} = \frac{V_{raw}}{k_T}
  \]

- 再计算 TDS，并应用校准系数 `kValue`
- 限幅：`0 ~ 1400 ppm`

## 5.3 浊度（`app_turbidity.c`）

- ADC 电压转传感器电压时使用分压系数：`TURBIDITY_DIVIDER_RATIO`（当前 2.0）
- 温度补偿项：`-0.0192 * (T - 25)`
- NTU 计算：

  \[
  NTU = (V + temp\_coefficient) \times (-865.68) + 3291.3 - 590
  \]

> 注意：若传感器输出可能超过 3.3V，必须在硬件上分压，避免损坏 ADC 引脚。

## 5.4 DS18B20（`app_ds18b20.c`）

- 采用 1-Wire 时序，读温度前执行转换命令
- 12 位分辨率时转换等待约 750ms，代码中等待 800ms

## 5.5 AHT20/BMP280（`app_aht20_bmp280.c`）

- AHT20：初始化、触发测量、读取 6 字节并解析 20-bit 湿度/温度数据
- BMP280：读取校准系数与原始值，执行官方补偿算法
- BMP280 结果增加限幅均值滤波，并计算海拔

---

## 6. 通信协议（USART2 JSON）

发送函数：`Comm_Send_Sensor_Data(...)`

典型数据帧（示例）：

```json
{"device_id":3,"pH":7.12,"TDS":356.4,"Tur":21.5,"Tem":24.8,"air_temp":26.1,"air_hum":61.4,"pressure":100845.2,"altitude":86.2,"status":"Active"}
```

字段含义：

- `device_id`：设备编号（宏 `COMM_DEVICE_ID`，当前 3）
- `pH`：酸碱度
- `TDS`：总溶解固体，单位 ppm
- `Tur`：浊度，单位 NTU
- `Tem`：水温，单位 ℃
- `air_temp`：空气温度，单位 ℃
- `air_hum`：空气湿度，单位 %
- `pressure`：气压（当前发送值为 Pa）
- `altitude`：海拔，单位 m
- `status`：设备状态

说明：每帧尾部包含 `\r\n`，便于上位机按行解析。

现状说明：当前实现中 `device_id` 在 JSON 内被写入两次（值一致，均为 `COMM_DEVICE_ID`），大多数 JSON 解析器会保留后一个同名键值。若你对协议做严格校验，建议后续在 `App/Src/app_json_comm.c` 中去重。

---

## 7. 构建与烧录

本仓库已配置 VS Code 任务（EIDE）：

- `build`：编译工程
- `flash`：烧录到设备
- `build and flash`：编译并烧录
- `rebuild`：全量重编
- `clean`：清理构建产物

操作方式：

1. 在 VS Code 中打开工作区
2. 执行对应任务（终端 -> 运行任务）
3. 连接目标板后执行 `flash` 或 `build and flash`

也可直接使用 Keil 打开：

- `MDK-ARM/WaterTest2.uvprojx`

---

## 8. 调试与联调建议

## 8.1 串口监视

- 调试串口：USART1（`printf` 输出）
- 业务串口：USART2（JSON 数据）
- 建议分别接两个串口工具观察

## 8.2 采样周期

- 全局变量 `report_interval` 控制主循环上报周期（单位 ms）
- 默认 `1500`

## 8.3 标定与参数

- pH：需结合标准缓冲液校准线性系数
- TDS：通过 `TDS_Set_K_Value()` 设置校准系数
- 浊度：确认电压分压比与传感器输出范围，必要时调整 `TURBIDITY_DIVIDER_RATIO`

---

## 9. 常见问题排查

1. **DS18B20 读数为 0**
   - 检查 `PC10` 连接与上拉
   - 检查供电与地线共地

2. **AHT20/BMP280 初始化失败**
   - 检查 I2C 地址、连线（PA8/PA9）
   - 排查 I2C 上拉电阻

3. **模拟值异常跳变**
   - 检查传感器供电稳定性与 ADC 地线噪声
   - 增加采样平均/中值窗口

4. **上位机收不到 JSON**
   - 确认监听串口是 USART2
   - 波特率必须 115200
   - 确认是否按 `\r\n` 分帧

---

## 10. 二次开发入口

推荐优先修改以下文件：

- 采样与算法：
  - `App/Src/app_ph_sensor.c`
  - `App/Src/app_tds_sensor.c`
  - `App/Src/app_turbidity.c`
- 协议与上报：
  - `App/Src/app_json_comm.c`
- 系统调度：
  - `Core/Src/main.c`

如果使用 CubeMX 重新生成代码，注意仅在 `/* USER CODE BEGIN */` 区块中保留自定义逻辑。

---

## 11. 版本说明

当前文档依据仓库 `main` 分支现有代码整理，重点描述“现状行为”。如后续调整引脚、协议字段或算法参数，请同步更新本 README。
