#pragma once 
#include <OneWire.h>
#include <DallasTemperature.h>

// DS18B20 数据引脚
#define PIN_18B20 2

// 创建 OneWire 和 DS18B20 对象
OneWire oneWire(PIN_18B20);
DallasTemperature sensors(&oneWire);

// 保存最近一次有效温度
float Temp18B20 = 25.0f;

// 温度转换状态
bool ds18b20Converting = false;

// 上次启动温度转换的时间
unsigned long ds18b20RequestTime = 0;

// 10 位分辨率转换时间约 188ms，留出一定余量
const unsigned long DS18B20_CONVERSION_MS = 200;

// ============================================================
// 初始化 DS18B20
// ============================================================
void init18B20() {
  // 初始化传感器
  sensors.begin();

  // 设置 10 位分辨率，转换时间约 188ms
  sensors.setResolution(10);

  // 关闭阻塞等待，避免 requestTemperatures() 等待转换完成
  sensors.setWaitForConversion(false);

  // 启动第一次温度转换
  sensors.requestTemperatures();

  ds18b20RequestTime = millis();
  ds18b20Converting = true;

  print("DS18B20 初始化完成，检测到设备: %d",
        sensors.getDeviceCount());
}


// ============================================================
// 获取 DS18B20 温度
// 返回最近一次有效温度
// ============================================================
float get18B20Temp() {
  // 判断本次温度转换是否完成
  if (ds18b20Converting &&
      millis() - ds18b20RequestTime >= DS18B20_CONVERSION_MS) {

    // 读取刚刚完成的温度转换结果
    float temp = sensors.getTempCByIndex(0);

    // 检查传感器读取是否正常
    if (temp != DEVICE_DISCONNECTED_C &&
        temp >= -55.0f && temp <= 125.0f) {
      Temp18B20 = temp;
    }

    // 启动下一次温度转换
    sensors.requestTemperatures();

    ds18b20RequestTime = millis();
    ds18b20Converting = true;
  }

  // 立即返回，不等待温度转换
  return Temp18B20;
}