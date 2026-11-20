#ifndef ENUM_AND_STRUCT_H
#define ENUM_AND_STRUCT_H

#include <Arduino.h>
#include <stdint.h>

// 原有全部 enum 和 struct 定义放这里


// ============================================================
// 设置模式
// ============================================================
enum SettingMode : uint8_t {
  NORMAL = 0,        // 非设置模式
  WELDING_TEMP = 1,  // 设置焊接温度
  LOW_TEMP = 2,      // 设置低温
  OFFSET = 3         // 设置温度偏移
};

// ============================================================
// 加热模式
// ============================================================
enum HeateMode : uint8_t {
  FULL = 0,   // 全速加热
  LIMIT = 1,  // 线性限制
  PID = 2     // PID 控制
};

// ============================================================
// 焊台工作模式
// ============================================================
enum WorkMode : int8_t {
  Mode_None = -1,
  WELDING = 0,
  LOWTEMP = 1,
  SLEEP = 2
};

// ============================================================
// EC11 旋转编码器事件
// ============================================================
enum EC11Event : uint8_t {
  EVENT_NONE = 0,
  EVENT_CW,
  EVENT_CCW,
  EVENT_CLICK,
  EVENT_DOUBLE_CLICK,
  EVENT_LONG,
  EVENT_SUPER_LONG
};

// ============================================================
// 烙铁头参数
// ============================================================
struct TipProfile {
  const char *name;
  float kp, ki, kd;
  uint8_t maxPower;
  int16_t tempOffset;
};

// ============================================================
// 温度采集参数
// ============================================================
struct TempParams {
  float adc_ref;
  float temp_ref;
  float cold_base;
  float slope;
};

// ============================================================
// 自动保存数据
// ============================================================
struct SaveData {
  bool isCoreTemp = false;
  float WeldingTemp = 100.0f;
  float lowTemp = 50.0f;
  float sleepTemp = 0.0f;
  float tipOffset = 60.0f;
};

// ============================================================
// PID 控制参数
// ============================================================
struct PIDConfig {
  bool needSave = false;

  float Kp = 15.0f;
  float Ki = 0.8f;
  float Kd = 8.0f;

  float THRES_BOOST_FULL = 30.0f;
  float THRES_BOOST_APPROACH = 10.0f;
  float THRES_HEATER_OFF = 0.0f;

  float THRES_INTEGRAL_SPLIT = 15.0f;

  float INTEGRAL_MAX_LIMIT = 100.0f;
  float INTEGRAL_MIN_LIMIT = -100.0f;

  float OVERHEAT_PROTECT_OFF = 30.0f;
};

// ============================================================
// 系统运行数据  // 系统当前的所有状态存储结构
// ============================================================
struct SystemData {
  unsigned long startTime = 0;       //开始时间 
  bool StopHeate = false;            //系统锁定
  SettingMode settingMode = NORMAL;  //系统模式
  WorkMode workMode = Mode_None;     //工作模式
  HeateMode heateMode = FULL;        //加热模式
  bool isInserted = true;            //插入
  unsigned long InSertTime = 0;      //插入 时间
  unsigned long IRKeepInTime = 0;    //本次插入时长
  float Temp18B20 = -126;            //主板温度
  float targetTemp = 100.0f;
  // 历史数据
  float orgADC = 0.0f;
  float orgTemp = 0.0f;
  double orgVolt = 0.0;
  // 当前数据
  float currentADC = 0.0f;
  float currentTemp = 0.0f;
  double currentVolt = 0.0;
  uint32_t pwm = 0;
  float pwmRate = 0.0f;
  String ErrCode;
};


#endif  // ENUM_AND_STRUCT_H