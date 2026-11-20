#pragma once
#include <Arduino.h>
#include "TaskAutoSave.h"
// 1. 先引入蜂鸣器头文件（里面定义了 playBeepById）
#include "TaskBuzzer.h" 
#include "FuncStandardPrint.h"
#include "GlobalParam.h"  
#define ENCODER_A 8
#define ENCODER_B 12
#define ENCODER_SW 9

const int T_SLOW = 150;
const int T_FAST = 40;
// 设定超长按阈值，例如 1500ms（可根据需要调整）
const unsigned long SUPER_LONG_THRESHOLD = 2000;
const unsigned long PRESS_THRESHOLD = 400;  // 双击判定时间窗口（毫秒）
//设置类型及保存参数


/************【00 回调 定义 】*****************/

// 定义温度区间结构体
struct EC11Setting {
  float min;        // 允许设置的最小值
  float max;        // 允许设置的最大值
  float step_slow;  // 步进值 (例如偏移模式通常需要更精细的 0.1 或 0.5)
  float step_med;   // slowstep 的 5倍
  float step_fast;  // slowstep 的 20倍  甚至更大
};

EC11Setting ec11Settings[3] = {
  // NORMAL: 正常显示 非设置模式
  { 0.0, 400.0, 1.0, 5.0, 20 },  // WELDING_TEMP:  焊接 (50-400°C)
  { 0.0, 350.0, 1.0, 5, 20 },    // LOW_TEMP: 低温待机 (0-350°C)
  { 0.0, 100.0, 0.5, 2.5, 10 }   // OFFSET: 温度偏移 (0-100°C, 步进0.5)
};

// ---------- 获取步进值（速度自适应）----------
float getStep(unsigned long interval, SettingMode mode) {
  if (interval <= T_FAST) {
    return ec11Settings[mode - 1].step_fast;
  } else if (interval <= T_SLOW) {
    return ec11Settings[mode - 1].step_med;
  } else {
    return ec11Settings[mode - 1].step_slow;
  }
}

// 当前的系统模式保存在 EC11 内部
extern volatile SettingMode curSettingMode ;// = NORMAL;
// 定义回调函数类型
// 2. 回调函数签名严格使用枚举类型   Event 事件类型   调整值 /按钮持续时长 ， 系统模式 mode
typedef void (*EC11Callback)(EC11Event Event, float stepValue, SettingMode mode);

// 保存回调指针
static EC11Callback globalEc11Callback = nullptr;

// 注册回调接口
inline void setEC11Callback(EC11Callback cb) {
  globalEc11Callback = cb;
}
//处理操作结果并回调   方向  速度模式  调整值
void AnyliseRotate2ChangeModeAndCallback(EC11Event event, unsigned long interval, float step_ora = 1.0f) {
  float step = step_ora * getStep(interval, curSettingMode);
  if (curSettingMode == NORMAL) {
    //跳转成 焊接温度设定
    curSettingMode = WELDING_TEMP;
  }
  if (globalEc11Callback) {
    globalEc11Callback(event, step, curSettingMode);
  }
}

void AnylisePush2ChangeModeAndCallback(EC11Event event, float value) {
  switch (event) {
    case EVENT_CLICK:
      if (curSettingMode == NORMAL) {
        curSettingMode = WELDING_TEMP;
        print("进入焊接温度设定");
      } else {
        curSettingMode = NORMAL;
        print("退出设定");
      }
      break;
    case EVENT_LONG:
      curSettingMode = LOW_TEMP;
      print("进入低温设定");
      break;
    case EVENT_DOUBLE_CLICK:
      curSettingMode = OFFSET;
      print("进入温差设定");
      break;
    default:
      break;
  }

  if (globalEc11Callback) {
    globalEc11Callback(event, value, curSettingMode);
  }
}

extern volatile int8_t encoderDelta;
extern volatile uint8_t lastEncState;
extern volatile int8_t subPos;
extern volatile uint8_t btnEvent;

extern TaskHandle_t TaskEncoderHandle;
extern TaskHandle_t TaskButtonHandle;

// 外部引用蜂鸣器的控制变量（用于在旋转时动态修改步长）
extern volatile uint16_t beepTargetCount;
extern volatile uint16_t currentCount;

static const int8_t KNOB_DIR[16] = {
  0, -1, 1, 0,
  1, 0, 0, -1,
  -1, 0, 0, 1,
  0, 1, -1, 0
};
float rotateVal = 0;
inline void IRAM_ATTR onEncoderChange() {
  uint8_t cur = (digitalRead(ENCODER_A) << 1) | digitalRead(ENCODER_B);
  if (cur != lastEncState) {
    uint8_t index = (lastEncState << 2) | cur;
    int8_t step = KNOB_DIR[index];
    lastEncState = cur;

    if (step != 0) {
      subPos += step;
      if (subPos >= 4) {
        encoderDelta += 1;
        rotateVal = 1;
        subPos = 0;
      } else if (subPos <= -4) {
        encoderDelta -= 1;
        rotateVal = -1;
        subPos = 0;
      }
    }
  }
}

inline void IRAM_ATTR onButtonChange() {
  if (digitalRead(ENCODER_SW) == LOW) {
    btnEvent = 1;
  } else {
    btnEvent = 2;
  }
}


inline void vEncoderTask(void *pvParameters) {
  unsigned long time_lastTurn = millis();
  int totalPosition = 0;

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(5);

  while (1) {
    int8_t localDelta = 0;
    portDISABLE_INTERRUPTS();
    localDelta = encoderDelta;
    encoderDelta = 0;
    portENABLE_INTERRUPTS();

    if (localDelta != 0) {
      unsigned long curTime = millis();
      unsigned long interval = curTime - time_lastTurn;
      time_lastTurn = curTime;

      const char *speedStr = "中速";
      if (interval <= T_FAST) speedStr = "快速";
      else if (interval > T_SLOW) speedStr = "慢速";

      totalPosition += localDelta;

      if (localDelta > 0) {
        playBeepById(1);  // 内部会自动 stopBeep 并无缝切换到新音效
        print("[旋钮任务]: 顺时针 (CW)  | 速度: %s (%lums) | 位置: %d", speedStr, interval, totalPosition);
        // 触发顺时针回调
        AnyliseRotate2ChangeModeAndCallback(EVENT_CW, interval, rotateVal);
      } else {
        playBeepById(2);  // 中速档：比如触发稍微低沉一点的音 (对应声音库编号 2: B_ROT_MINUS)
        print("[旋钮任务]: 逆时针 (CCW) | 速度: %s (%lums) | 位置: %d", speedStr, interval, totalPosition);
        // 触发逆时针回调
        AnyliseRotate2ChangeModeAndCallback(EVENT_CCW, interval, rotateVal);
      }

      // 根据速度动态调整蜂鸣器响应步长
      portDISABLE_INTERRUPTS();
      if (interval <= T_FAST) {
        currentCount += abs(localDelta);
      } else if (interval <= T_SLOW) {
        currentCount += abs(localDelta);
      } else {
        currentCount += abs(localDelta);
      }
      portENABLE_INTERRUPTS();
    }

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

inline void vButtonTask(void *pvParameters) {
  unsigned long time_press_start = 0;
  bool isButtonHolding = false;
  bool longPressTriggered = false;  // 超长按锁定标记，确保抬起前仅触发一次

  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(10);
  static int clickCount = 0;                 // 连击次数计数
  static unsigned long lastReleaseTime = 0;  // 上一次松开的时间
  while (1) {
    uint8_t localSw = 0;
    portDISABLE_INTERRUPTS();
    localSw = btnEvent;
    btnEvent = 0;
    portENABLE_INTERRUPTS();

    unsigned long curTime = millis();

    if (localSw == 1) {
      time_press_start = curTime;
      isButtonHolding = true;
      longPressTriggered = false;  // 每次按下重新解锁超长按标记
      print("[按键任务]: 按下");
    } 
    if (isButtonHolding) {
      unsigned long holdDuration = curTime - time_press_start; 
      // 超长按检测：按住时间达到阈值，且本轮按下期间还未触发过
      if (!longPressTriggered && holdDuration >= SUPER_LONG_THRESHOLD) {
        playBeepById(6);            // 触发超长按音效
        longPressTriggered = true;  // 锁定：抬起前绝不重复触发
        print("[按键任务]: 触发超长按 (时长: %lums)", holdDuration);
        // 触发超长按回调
        AnylisePush2ChangeModeAndCallback(EVENT_SUPER_LONG, (float)holdDuration);
        clickCount = 0;
      } 
      static unsigned long lastLogTime = 0;
      if (holdDuration >= 1000 && curTime - lastLogTime >= 1000) {
        print("[按键任务]: 持续按住... 已按压 %lums", holdDuration);
        lastLogTime = curTime;
      }
    }

    if (localSw == 2) {  //抬起
      unsigned long dur = curTime - time_press_start;
      isButtonHolding = false;
      if (dur >= 50) {
        print("[按键任务]: 松开 | 时长: %lums", dur);
        if (!longPressTriggered) {  //未触发超长按
          // 假设 400ms 以下为点按，400ms 及以上为中长按
          if (dur < PRESS_THRESHOLD) {  //短按事件 1)点按 2）1秒内 2次点按  
            // 小于 400ms，属于短按候选（可能是单击，也可能是双击的第一下）
            unsigned long interval = curTime - lastReleaseTime; 
            if (clickCount == 1 && interval <= PRESS_THRESHOLD) { //点按过一次 并且本次点按中间间隔没有超过 PRESS_THRESHOLD
              // 成功在窗口期内按了第二下 -> 触发双击！
              clickCount = 0;   // 重置计数
              playBeepById(4);  // 假设双击用音效 ID 4（可根据需要修改）
              print("[按键任务]: 触发双击");
              AnylisePush2ChangeModeAndCallback(EVENT_DOUBLE_CLICK, (float)dur);  // 需要在枚举中加一个 EVENT_DOUBLE_CLICK
            } else {
               clickCount = 1;
              playBeepById(3);  // 点按抬起触发
              print("[按键任务]: 触发点按音效");
              // 触发点按回调
              AnylisePush2ChangeModeAndCallback(EVENT_CLICK, (float)dur);
            }
            lastReleaseTime = curTime; 
          } else {            //中长按事件
            playBeepById(5);  // 中长按抬起触发
            print("[按键任务]: 触发中长按音效");
            // 触发中长按回调
            AnylisePush2ChangeModeAndCallback(EVENT_LONG, (float)dur);
          }
        }
      }
    }
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}



// 编码器模块初始化接口
inline void initEC11System() {
   curSettingMode  = NORMAL;
  pinMode(ENCODER_A, INPUT_PULLUP);
  pinMode(ENCODER_B, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);

  lastEncState = (digitalRead(ENCODER_A) << 1) | digitalRead(ENCODER_B);
  subPos = 0;

  attachInterrupt(digitalPinToInterrupt(ENCODER_A), onEncoderChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_B), onEncoderChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_SW), onButtonChange, CHANGE);

  xTaskCreatePinnedToCore(vEncoderTask, "EncoderTask", 4096, NULL, 2, &TaskEncoderHandle, 0);
  xTaskCreatePinnedToCore(vButtonTask, "ButtonTask", 4096, NULL, 2, &TaskButtonHandle, 0);
}