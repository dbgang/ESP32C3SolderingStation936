//Arduino-ESP32 Core 3.x，PWM 分辨率可以设置为 1～14 位，12 位是常用配置。
// 配置 PWM：25kHz，12位分辨率 ledcAttach(5, 25000, 12);
// 分辨率	占空比级数	单级精度
// 8  位	256	  0.3906%
// 10 位	1024	0.0977%
// 12 位	4096	0.0244%
// 13 位	8192	0.0122%
// 14 位	16384	0.0061%

// 对于你的烙铁控制，我建议：
// - 12 位：优先选择，精度和频率兼顾。
// - 13～14 位：没有太大实际收益。
// - 8 位：也足够普通温控使用。
// 最终建议：25kHz、12 位，作为烙铁 PWM 的默认配置。

#pragma once
#include "GlobalParam.h"  //引用了EnumAndStruct
#include "GlobalFunc.h"   //引用了EnumAndStruct
#include "FuncStandardPrint.h"
#include "FuncCircularStorage.h"  //ADC 记录
#include "Func18B20.h"
#include "FuncADC.h"
#include <EnumAndStruct.h>
// --- 引脚与宏定义 ---
#define PWM_A1321 6  // 936加热 引flag

const int FREQ = 25000;                    // 25kHz 高频，无啸叫静音模式 [1]
const int RESOLUTION = 8;                  //这里用 8 位   12位分辨率（0~4095），是25kHz下的硬件极限 [1]
const unsigned long sleepTime = 300000UL;  // 休眠时间
// --- 全局变量与参数定义 ---

// ============================================================
// 创建 PID 参数实例，自动采用上述默认值
// ============================================================
extern PIDConfig pidConfig;
// ============================================================
// PID 控制器运行状态
// ============================================================
float errorSum = 0.0f;   // 积分累计值
float lastError = 0.0f;  // 上一次误差

// --- 3. PWM 输出限幅 ---
int PWM_MAX_OUTPUT = 255;                                    // 255;              // 最大 PWM 输出值
const int PWM_MIN_OUTPUT = 0;                                // 最小 PWM 输出值
const int PWM_APPROACH_MIN_VAL = (int)(PWM_MAX_OUTPUT * 1);  // 线性减速区间的起始最低功率  最大功率的72%   原始值185  升温慢 卡在低1度

// --- 4. 运行参数 ---
const unsigned long PID_CYCLE_MS = 50;  // PID 计算周期 (50ms)
//  冷态加热最高时长 全功率最长运行 30 秒
const unsigned long MaxPowerKeepTime = 30000;  // 满功率最长持续安全时间 (30秒)
unsigned long MaxPowerBeginTime = 0;

const int SAFETY_MONITOR_PWM = PWM_APPROACH_MIN_VAL;
const float SAFETY_MIN_2SEC_RISE = 4.0;
extern volatile unsigned long setPwmTime;  // 必须带 volatile，否则类型依然冲突
extern SystemData current;                 //【PWM 记录】当前所有的状态
extern volatile SettingMode curSettingMode;
extern volatile bool isInserted;         //【外部】是否休眠
extern volatile unsigned long IRInTime;  //上次操作时间  millis()
extern volatile bool hasUsed;            //  是否用过
extern volatile bool hasSetted;          //  是否设置过
extern String ErrCode;                   //不能 volatile
extern SaveData saved;
// --- FreeRTOS 任务句柄 ---
TaskHandle_t HeaterTaskHandle = NULL;

void getCurrent() {
  //【0】记录开始时间
  if (current.startTime == 0) {
    current.startTime = millis();  //记录开始时间
  }

  //【1】 外部信息
  current.isInserted = isInserted;  //插入
  current.InSertTime = IRInTime;    //插入sh
  current.IRKeepInTime = millis() - current.InSertTime;
  current.Temp18B20 = get18B20Temp();

  //【1.2】系统模式
  current.settingMode = curSettingMode;
  //【1.3】工作模式  // 当前没有睡眠请求，始终处于焊接模式
  //非第一次 切插入
  if (isInserted && hasUsed) { 
    // 已经进入睡眠流程，根据红外保持时间切换低温保持和完全休眠
    if (current.IRKeepInTime < sleepTime) { 
      current.workMode = LOWTEMP;
    } else { 
      hasSetted = true;
      current.workMode = SLEEP;
    }
  } else {  //第一次 或 未插入
    current.workMode = WELDING;
  }
  // current.heateMode = FULL;      //加热模式 PWM Control 中赋值
  //【1.3】目标温度设定
  switch (current.workMode)  //bool isCoreTemp = false;
  {
    case SLEEP:
      current.targetTemp = saved.sleepTemp;  // 休眠温度（请替换为你的实际变量或函数）
      break;

    case LOWTEMP:
      current.targetTemp = saved.lowTemp;
      break;

    case WELDING:
    default:
      current.targetTemp = saved.WeldingTemp;
      break;
  }

  // 【2.1】历史数据
  current.orgADC = current.currentADC;
  current.orgTemp = current.currentTemp;
  current.orgVolt = current.currentVolt;

  // 【3.1】当前数据  注意 current.currentADC, current.Temp18B20 调用的getCurrentADC() 和  get18B20Temp();  只能调用一次
  current.currentADC = getCurrentADC();
  current.currentTemp = ADC2TempV2(current.currentADC, current.Temp18B20, tempParams);
  current.currentVolt = getCurrentVoltage(currentADC);
  // 【1.1】主板温度报错及错误赋值
  if (current.currentADC < 500) {
    current.ErrCode = "adcL";
  } else if (current.currentADC > 40000) {  //451911  653   40000 大约在500度
    current.ErrCode = "adcH";
  } else if (!hasSetted && current.currentTemp - current.targetTemp > 50.0f) { //注意这里hasSetted排除 EC11、红外对射 、延时休眠
    current.ErrCode = "adcO";  //这里 主板问题最先显示
  } else if (current.Temp18B20 > 60.0f) {
    current.ErrCode = "E01";  //这里 主板问题最先显示
  } else {
    current.ErrCode = ErrCode;
  }
  //【4】是否需要加热
  if (current.ErrCode == "" && current.targetTemp > current.currentTemp) {
    current.StopHeate = false;
  } else {
    current.StopHeate = true;
  }
  current.pwm = ledcRead(PWM_A1321);
  current.pwmRate = current.pwm / PWM_MAX_OUTPUT;
}


//int lastpwm = 0;
// 设置 PWM 输出
void setPWM(int pwm) {
  setPwmTime = millis();    // 記錄當前時間（毫秒）  micros() 这个是微 秒
  if (current.StopHeate) {  // 最高优先级  最高优先级
    ledcWrite(PWM_A1321, 0);
    //current.pwm = 0;
  } else {
    if (current.pwm == pwm && pwm == 255 && millis() - MaxPowerBeginTime > MaxPowerKeepTime)  //热失控  全功率255加热时间过长
    {
      current.ErrCode = "E255";
      ledcWrite(PWM_A1321, 0);
    }
    if (current.pwm == 0 || current.pwm != pwm) {  //停止的一定执行上次的和这次 的不一致也要执行
      MaxPowerBeginTime = millis();                // PWM 变更则等级 时间
      ledcWrite(PWM_A1321, pwm);
      //  print("PWM:%lu", current.pwm);
      //  current.pwm = pwm;
    } else {
      //print("CURRENT pwm = last: %d", pwm );
    }
  }
}


//精准控温逻辑
int getPIDPWM(float target, float current) {
  float error = target - current;
  // 1. 积分分离逻辑：温差大时不累加
  if (abs(error) < pidConfig.THRES_INTEGRAL_SPLIT) {
    errorSum += error;
  } else {
    errorSum = 0;
  }
  // 限制积分影响
  errorSum = constrain(errorSum, pidConfig.INTEGRAL_MIN_LIMIT, pidConfig.INTEGRAL_MAX_LIMIT);

  // 2. 微分逻辑
  float dError = error - lastError;
  lastError = error;

  // 3. 计算 PID
  int pidOut = (pidConfig.Kp * error) + (pidConfig.Ki * errorSum) + (pidConfig.Kd * dError);

  // 4. 输出限幅
  return constrain(pidOut, PWM_MIN_OUTPUT, PWM_MAX_OUTPUT);
}
// 【升温逻辑 全速和线性】快速升温逻辑  全功率和线性功率
int getBoostPWM(float target, float current) {
  float diff = target - current;
  if (diff > pidConfig.THRES_BOOST_FULL) {
    return PWM_MAX_OUTPUT;  // 远超阈值：地板油，全力冲刺
  } else if (diff > pidConfig.THRES_BOOST_APPROACH) {
    // 30- 10度之间：线性减速
    // 从 PWM_MAX_OUTPUT 降到 PWM_APPROACH_MIN_VAL 左右
    return map(diff, pidConfig.THRES_BOOST_APPROACH, pidConfig.THRES_BOOST_FULL, PWM_APPROACH_MIN_VAL, PWM_MAX_OUTPUT);
  }
  return -1;  // 阈值以内交回给 PID 慢速进库
}

int finalPWM = 0;
int i = 0;


// --- FreeRTOS 任务入口函数 ---
void heaterTaskCode(void *pvParameters) {
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = pdMS_TO_TICKS(PID_CYCLE_MS);  // 转换为 FreeRTOS 节拍 (50ms)

  // 初始化任务的唤醒时间戳
  xLastWakeTime = xTaskGetTickCount ? xTaskGetTickCount() : 0;
  // 注：为兼容不同版本，直接使用以下标准阻塞延时更简便：
  // TickType_t xLastWakeTime = xTaskGetTickCount();
  while (1) {
    // 执行温控核心逻辑
    getCurrent();  //获取系统状态 进行安全检查
    //重置判定变量值
    finalPWM = 0;
    //--- 逻辑判断 ---
    if (hasUsed && millis() - current.startTime < 1000UL) {
      finalPWM = getBoostPWM(current.targetTemp, current.currentTemp);
      current.heateMode = (current.targetTemp - current.currentTemp > pidConfig.THRES_BOOST_FULL) ? FULL : LIMIT;
      // print("[1]targetTemp :%0.1f ,currentTemp :%0.1f,pwm: %d ", current.targetTemp, current.currentTemp, finalPWM);
    } else {
      finalPWM = getPIDPWM(current.targetTemp, current.currentTemp);
      current.heateMode = PID;
      //  print("[2]targetTemp :%0.1f ,currentTemp :%0.1f,pwm: %d ", current.targetTemp, current.currentTemp, finalPWM);
    }
    setPWM(finalPWM);
    if (i++ > 5) {
      printSystemLog(current);
      i = 0;
    }
    // 精准周期延时（比 vTaskDelay 更稳定，不累积时间误差）
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}


// --- 初始化方法（包含 PWM 初始化与任务创建） ---
void initPWMAndTask() {
  // 初始化并绑定 LEDC 通道（适用于 Arduino ESP32 Core v3.x）
  if (!ledcAttach(PWM_A1321, FREQ, RESOLUTION)) {
    print("PWM 设置失败!");
  } else {
    print("PWM 设置成功!");
  }

  // int pwmOut = map(adcValue, 0, 32767, 0, 16383);
  // 创建加热控制独立 Task，分配核心 1（或由系统调度），栈大小 2048，优先级 3
  BaseType_t result = xTaskCreate(
    heaterTaskCode,       // 任务入口函数
    "HeaterControlTask",  // 任务名称（调试用）
    4096,                 // 堆栈大小 (bytes)
    NULL,                 // 传递参数
    9,                    // 任务优先级
    &HeaterTaskHandle     // 任务句柄（注意：xTaskCreate 没有末尾的核心参数）
  );


  if (result == pdPASS) {
    print("Heater Task created successfully!");
  } else {
    print("Failed to create Heater Task!");
  }
}
