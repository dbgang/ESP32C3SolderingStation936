#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// ============================================================
// ADS1115 配置
// ADS1115 支持以下采样率：
// 采样率	单次转换时间
// 8 SPS	125 ms
// 16 SPS	62.5 ms
// 32 SPS	31.25 ms
// 64 SPS	15.625 ms
// 128 SPS	7.8125 ms
// 250 SPS	4 ms
// 475 SPS	2.105 ms
// 860 SPS	1.163 ms
// ============================================================
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define ADS1115_ADDRESS 0x48
// ADS1115 对象
Adafruit_ADS1115 ads;
// 当前 ADC 原始值
int16_t currentADC = 0; 
// 错误代码
extern String ErrCode; 
// ============================================================
// 初始化 ADS1115
// ============================================================
bool initADC() {
  // 初始化 I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  // 初始化 ADS1115，地址 0x48
  if (!ads.begin(ADS1115_ADDRESS)) {
    ErrCode = "E1115";
    return false;
  }
  ads.setGain(GAIN_TWOTHIRDS);
  ads.setDataRate(RATE_ADS1115_128SPS);
  // 设置 PGA 增益
  // GAIN_EIGHT：量程 ±0.512V
  // ads.setGain(GAIN_EIGHT);
  // 设置采样率 128 SPS
  //ads.setDataRate(RATE_ADS1115_128SPS);

  // 清除错误代码
  ErrCode = "";

  return true;
}
//转换 电压值
float getCurrentVoltage(int16_t currentADC) {
  return ads.computeVolts(currentADC);
}

// ============================================================
// 获取当前 ADC 原始值（带本地缓存）
// 缓存有效期：20ms
// ============================================================
int16_t getCurrentADC() {
  // 静态变量：函数多次调用时保留上一次的缓存数据
  static int16_t cachedADC = 0;
  static unsigned long lastReadTime = 0;
  static bool cacheValid = false;
  // 获取当前毫秒数
  unsigned long now = millis();
  // 缓存有效期内，直接返回上次读取值
  if (cacheValid && (now - lastReadTime < 20)) {
    return cachedADC;
  }
  // 缓存过期，重新读取 ADS1115 AIN0
  int16_t raw = ads.readADC_SingleEnded(0);
  // 检查 ADC 是否异常
  if (raw < 0 || raw >= 32700) {
    ErrCode = "E1321";
    // 异常时返回上次有效值
    return cachedADC;
  }
  // 保存最新有效 ADC 数据
  cachedADC = raw;
  currentADC = raw;
  // 更新缓存时间和有效状态
  lastReadTime = now;
  cacheValid = true;
  return cachedADC;
}