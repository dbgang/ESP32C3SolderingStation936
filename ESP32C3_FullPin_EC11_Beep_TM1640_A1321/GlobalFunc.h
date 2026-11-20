#pragma once  
#include <EnumAndStruct.h>
 
 
//********【5   通用方法 】********** 
// ADC 转换电压 (将参数改为 float，兼容传入的 orgADC 和 ADC 采样值)
double ADC2Voltage(float adc) {
  // 4. 换算电压
  double voltage = (double)adc * (2.048 / 8388607.0);
  return voltage;
}


// 结合烙铁头特性的温度补偿计算函数【非原始】
float getRealTipTemp(float coreTemp, uint8_t currentPWM, TipProfile* currentProfile) {
  float tipTemp = coreTemp;

  // 1. 静态温差补偿（不同头子本身的热阻不同）
  tipTemp -= currentProfile->tempOffset;

  // 2. 动态负载补偿（关键所在！）
  // 当 PWM 开得很大（说明正在大功率加热或掉温），
  // 内部核心温度明显高于尖端，需要把内部的“超前量”扣除一部分
  float powerRatio = (float)currentPWM / 255.0f;  // 0.0 到 1.0

  if (currentProfile == &profiles[0]) {
    // 假设 profiles[0] 是大头（热容量大，延迟显著）
    // 大头在大功率加热时，核心与尖端的温差非常大
    tipTemp -= (powerRatio * 35.0f);
  } else {
    // 假设 profiles[1] 是小头（热阻小，延迟相对较小）
    tipTemp -= (powerRatio * 15.0f);
  }
  return tipTemp;
}

// ============================================================
// ADC 温度校准：全温区实测查表 + 线性插值
//
// 校准范围：22.2℃ ～ 304.2℃
// 数据来源：本次提供的 62 组 ADC / MAX31855 实测数据
//
// 优点：
// 1. 不假设 ADC 与温度完全线性
// 2. 自动适应不同温区的斜率变化
// 3. 不需要人为设置三个温区的斜率
// 4. 保留原来的 ADC2Temp() 调用接口
// ============================================================

 
float ADC2Temp(float adc, float temp18B20, const TempParams& params) {

  // ----------------------------------------------------------
  // ADC 温度校准表
  // 每一行分别是：{ADC计数值, MAX31855实测温度}
  // 数据来源：本次实际采集的16组数据
  // 必须按照 ADC 从小到大排列
  // ----------------------------------------------------------
 


  static const float calibrationTable[][2] = {
  { 4898.0f,  21.0f },
  { 6009.0f,  60.0f },
  { 7212.0f, 146.0f },
  { 8551.0f, 213.0f },
  { 8670.0f, 213.0f },
  { 8819.0f, 236.0f },
  { 9043.0f, 242.0f },
  { 9451.0f, 254.0f },
  { 9831.0f, 273.0f },
  { 10305.0f, 307.0f },
  { 10983.0f, 335.0f },
  { 11184.0f, 354.0f },
  { 11372.0f, 360.0f },
  { 12062.0f, 375.0f },
  { 12801.0f, 401.0f },
  { 13309.0f, 438.0f }
};


  // 获取校准表数据数量
  constexpr int tableSize = sizeof(calibrationTable) / sizeof(calibrationTable[0]);

  float baseTemp = 0.0f;

  // ----------------------------------------------------------
  // 超出校准范围时，限制在首尾校准值
  // 避免低温和高温区域进行不可靠的线性外推
  // ----------------------------------------------------------
  if (adc <= calibrationTable[0][0]) {
    baseTemp = calibrationTable[0][1];
  }
  else if (adc >= calibrationTable[tableSize - 1][0]) {
    baseTemp = calibrationTable[tableSize - 1][1];
  }
  else {

    // --------------------------------------------------------
    // 查找 ADC 所在的两个相邻校准点
    // --------------------------------------------------------
    for (int i = 0; i < tableSize - 1; i++) {

      float adc1 = calibrationTable[i][0];
      float temp1 = calibrationTable[i][1];

      float adc2 = calibrationTable[i + 1][0];
      float temp2 = calibrationTable[i + 1][1];

      if (adc >= adc1 && adc <= adc2) {

        // 两点之间进行线性插值
        float ratio = (adc - adc1) / (adc2 - adc1);

        baseTemp = temp1 + ratio * (temp2 - temp1);

        break;
      }
    }
  }

  // ----------------------------------------------------------
  // 18B20 环境温度补偿
  // 校准数据采集时的环境基准温度为 cold_base
  // ----------------------------------------------------------
  return baseTemp + (temp18B20 - params.cold_base);
}


// 【ADC转温度】 物理计算公式：仅负责数学运算，不负责逻辑判断
// 无需修改函数体，只需将 params 初始化为以下校准值：
 // ADC 转换为温度，并进行环境温度补偿
float ADC2TempHis(float adc, float temp18B20, const TempParams& params) {
  float base = (adc - params.adc_ref) * params.slope + params.temp_ref;
  return base + (temp18B20 - params.cold_base);
}
/**
 * @brief 动态计算单边剔除数量 (阶梯式截断算法)
 * @param size 总样本数
 * @return int 单边需要剔除的点数
 */
int getTrimCount(int size) {
  // 1. 极小样本区：不去极值，保证基本响应
  if (size < 5) {
    return 0;
  }

  // 2. 小样本区 (5-9个)：去 20% (10% 其实在小样本下很难取整，直接给 1)
  if (size < 10) {
    return 1;  // 5个点去1首1尾剩3个，刚好
  }

  // 3. 标准样本区 (10-19个)：去 30%
  if (size < 20) {
    // 比如 10个点 -> 3个; 15个点 -> 4个
    return (int)(size * 0.3f);
  }

  // 4. 大样本区 (20个以上)：去 35% 或更高
  // 此时干扰点可能成堆出现，可以更“狠”一点
  int trim = (int)(size * 0.35f);

  // 安全兜底：确保两边剔除后，中间至少剩 4 个点用于平均
  if (size - 2 * trim < 4) {
    trim = (size - 4) / 2;
  }

  return (trim > 0) ? trim : 0;
}
/**
 * @brief 截断平均值算法 (Trimmed Mean)
 * 逻辑：排序后去掉两端异常值，取中间平稳数据 【内含有 根据采集数量 计算 去极值的数】
 * @param input 原始采样数组
 * @param size 数组总长度 (如 10)
 * // @param trimCount 两端各去掉的点数 (如 3)
 */
float calculateTrimmedMean(float* input, int size) {
  int trimCount = getTrimCount(size);
  // 1. 安全校验：确保去掉后还有数据剩下
  if (size <= trimCount * 2)
    return 0.0;

  // 2. 局部备份，避免修改原始采样序列
  float temp[size];
  memcpy(temp, input, size * sizeof(float));

  // 3. 排序 (由于只有10个点，使用插入排序效率最高且不占堆栈)
  for (int i = 1; i < size; i++) {
    float key = temp[i];
    int j = i - 1;
    while (j >= 0 && temp[j] > key) {
      temp[j + 1] = temp[j];
      j--;
    }
    temp[j + 1] = key;
  }

  // 4. 求中间段的和
  float sum = 0;
  int validPoints = size - (2 * trimCount);
  for (int i = trimCount; i < size - trimCount; i++) {
    sum += temp[i];
  }

  // 5. 返回平均值
  return sum / (float)validPoints;
}