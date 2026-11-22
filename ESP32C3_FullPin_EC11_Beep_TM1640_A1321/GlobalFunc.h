#pragma once
#include <EnumAndStruct.h>

enum class TempStatus : uint8_t {
  Ok = 0,           // 校准区间内正常插值
  ExtrapLow,        // 低于最低校准点，按首段斜率线性外推（如冬季低温）
  ExtrapHigh,       // 高于最高校准点，按末段斜率线性外推（轻微超温）
  FaultOpenOrLow,   // 严重偏低 / 传感器断线或异常
  FaultShortOrHigh  // 严重偏高 / 传感器短路或严重超温
};

struct TempResult {
  float temp;
  TempStatus status;
};

// 清洗后的 15 组校准表 
static constexpr float calibrationTable[][2] = {
  { 4898.0f, 21.0f },
  { 6009.0f, 60.0f },
  { 7212.0f, 146.0f },
  { 8551.5f, 213.0f },
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

float ADC2TempV2(float adc, float temp18B20, const TempParams& params) {
  constexpr int tableSize = sizeof(calibrationTable) / sizeof(calibrationTable[0]);
  if (tableSize < 2) {
    return 0.0f;
  }

  float baseTemp = 0.0f;

  // 1. 低于最小校准点：使用首段（index 0 -> 1）斜率线性外推
  if (adc < calibrationTable[0][0]) {
    float adc0 = calibrationTable[0][0];
    float temp0 = calibrationTable[0][1];
    float adc1 = calibrationTable[1][0];
    float temp1 = calibrationTable[1][1];

    float dAdc = adc1 - adc0;
    float slope = (dAdc != 0.0f) ? ((temp1 - temp0) / dAdc) : 0.0f;
    baseTemp = temp0 + (adc - adc0) * slope;
  }
  // 2. 高于最大校准点：使用尾段（index tableSize-2 -> tableSize-1）斜率线性外推
  else if (adc > calibrationTable[tableSize - 1][0]) {
    float adcPrev = calibrationTable[tableSize - 2][0];
    float tempPrev = calibrationTable[tableSize - 2][1];
    float adcLast = calibrationTable[tableSize - 1][0];
    float tempLast = calibrationTable[tableSize - 1][1];

    float dAdc = adcLast - adcPrev;
    float slope = (dAdc != 0.0f) ? ((tempLast - tempPrev) / dAdc) : 0.0f;
    baseTemp = tempLast + (adc - adcLast) * slope;
  }
  // 3. 校准表范围内：相邻两点线性插值
  else {
    baseTemp = calibrationTable[tableSize - 1][1];
    for (int i = 0; i < tableSize - 1; i++) {
      float adc1 = calibrationTable[i][0];
      float temp1 = calibrationTable[i][1];
      float adc2 = calibrationTable[i + 1][0];
      float temp2 = calibrationTable[i + 1][1];

      if (adc >= adc1 && adc <= adc2) {
        float dAdc = adc2 - adc1;
        float ratio = (dAdc != 0.0f) ? ((adc - adc1) / dAdc) : 0.0f;
        baseTemp = temp1 + ratio * (temp2 - temp1);
        break;
      }
    }
  }

  // 4. 18B20 环境温度补偿
  float finalTemp = baseTemp + (temp18B20 - params.cold_base);

  // 5. 安全温度范围限幅（可根据实际加热芯与温控安全阈值调整）
  constexpr float MIN_SAFE_TEMP = -20.0f;
  constexpr float MAX_SAFE_TEMP = 500.0f;
  if (finalTemp < MIN_SAFE_TEMP) finalTemp = MIN_SAFE_TEMP;
  if (finalTemp > MAX_SAFE_TEMP) finalTemp = MAX_SAFE_TEMP;

  return finalTemp;
}

// 硬件故障保护阈值（按 ADS1115 实际量程调整
constexpr float FAULT_ADC_MIN = 500.0f; // 低于此值判定断线/低端异常
constexpr float FAULT_ADC_MAX = 30000.0f; // 高于此值判定短路/饱和异常
constexpr float INVALID_TEMP = -999.0f;
TempResult calcTempSmart(float adc) {
  constexpr int N = sizeof(calibrationTable) / sizeof(calibrationTable[0]);

  // 1. 非法值与严重硬件故障拦截
  if (!std::isfinite(adc) || adc < FAULT_ADC_MIN) {
    return { INVALID_TEMP, TempStatus::FaultOpenOrLow };
  }
  if (adc > FAULT_ADC_MAX) {
    return { INVALID_TEMP, TempStatus::FaultShortOrHigh };
  }

  // 2. 低于最低校准点 (adc < 4898)：使用前两个点 (4898->21, 6009->60) 的斜率向下线性外推
  //    斜率约 0.0351 °C/ADC，可准确反映 0°C ~ 21°C 的室温变化
  if (adc < calibrationTable[0][0]) {
    float slopeLow = (calibrationTable[1][1] - calibrationTable[0][1]) / (calibrationTable[1][0] - calibrationTable[0][0]);
    float temp = calibrationTable[0][1] + (adc - calibrationTable[0][0]) * slopeLow;
    return { temp, TempStatus::ExtrapLow };
  }

  // 3. 高于最高校准点 (adc > 13309)：使用最后两个点 (12801->401, 13309->438) 的斜率向上线性外推
  //    斜率约 0.0728 °C/ADC，超过 438°C 后仍能连续反映升温趋势，不会卡死
  if (adc > calibrationTable[N - 1][0]) {
    float slopeHigh = (calibrationTable[N - 1][1] - calibrationTable[N - 2][1]) / (calibrationTable[N - 1][0] - calibrationTable[N - 2][0]);
    float temp = calibrationTable[N - 1][1] + (adc - calibrationTable[N - 1][0]) * slopeHigh;
    return { temp, TempStatus::ExtrapHigh };
  }

  // 4. 校准区间内 [4898, 13309]：二分查找相邻点进行分段线性插值
  int left = 0;
  int right = N - 1;
  while (right - left > 1) {
    int mid = left + (right - left) / 2;
    if (adc >= calibrationTable[mid][0]) {
      left = mid;
    } else {
      right = mid;
    }
  }

  float adc1 = calibrationTable[left][0];
  float temp1 = calibrationTable[left][1];
  float adc2 = calibrationTable[right][0];
  float temp2 = calibrationTable[right][1];

  float temp = temp1 + (adc - adc1) * (temp2 - temp1) / (adc2 - adc1);
  return { temp, TempStatus::Ok };
}

TempResult ADC2TempV2Smart(float adc, float temp18B20, const TempParams& params) {
  TempResult res = calcTempSmart(adc);

  // 传感器本身已判为故障，直接返回，不叠加补偿
  if (res.status == TempStatus::FaultOpenOrLow || res.status == TempStatus::FaultShortOrHigh) {
    return res;
  }

  // 若 18B20 读数有效，按原逻辑叠加环境/冷端基准温度补偿
  if (std::isfinite(temp18B20) && std::isfinite(params.cold_base)) {
    res.temp += (temp18B20 - params.cold_base);
  }

  return res;
}


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


float ADC2TempV2His(float adc, float temp18B20, const TempParams& params) {

  // ----------------------------------------------------------
  // ADC 温度校准表
  // 每一行分别是：{ADC计数值, MAX31855实测温度}
  // 数据来源：本次实际采集的16组数据
  // 必须按照 ADC 从小到大排列
  // ----------------------------------------------------------

  // 获取校准表数据数量
  constexpr int tableSize = sizeof(calibrationTable) / sizeof(calibrationTable[0]);

  float baseTemp = 0.0f;

  // ----------------------------------------------------------
  // 超出校准范围时，限制在首尾校准值
  // 避免低温和高温区域进行不可靠的线性外推
  // ----------------------------------------------------------
  if (adc <= calibrationTable[0][0]) {
    baseTemp = calibrationTable[0][1];
  } else if (adc >= calibrationTable[tableSize - 1][0]) {
    baseTemp = calibrationTable[tableSize - 1][1];
  } else {

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