#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "EnumAndStruct.h"
#include "FuncStandardPrint.h"
#include "TaskEC11.h"
#include "TM1640ColorLed.h"

// ----------------------------------------
// 任务模式枚举
// ----------------------------------------
enum DisplayMode {
  DIS_NORMAL,    // 正常常亮刷新
  DIS_FLASH,     // 闪烁模式
  DIS_COLORLED,  //显示三色LED 模式
};
const unsigned long flashInterval = 400;  //闪烁间隔 毫秒
// 【修改这里】：用带前缀的 static const 替代宏定义，彻底避免与 Wire.h 等库冲突
static uint8_t TM1640_DIN_PIN = 18;
static uint8_t TM1640_SCL_PIN = 19;
static uint8_t brightness = 0x8A;  // 亮度
// TM1640 固定支持最大 16 位 (地址 0xC0 ~ 0xCF)
#define TM1640_TOTAL_DIGITS 16
// 1. 定义段码缓存数组（直接对应硬件上的每一位）
static uint8_t text[TM1640_TOTAL_DIGITS] = { 0 };
static uint8_t target[TM1640_TOTAL_DIGITS] = { 0 };
static uint8_t last[TM1640_TOTAL_DIGITS] = { 0 };
// 严格按要求的 Screen 结构体（不增减字段）  每个数码管定义一个
typedef struct {
  uint8_t startAddr = 0xC0;                     // 起始地址 (例如 0xC0)
  uint8_t digitCount = 4;                       // 数码位数
  DisplayMode mode = DIS_NORMAL;                // 独立显示模式
  uint8_t buffer[TM1640_TOTAL_DIGITS] = { 0 };  // 加上了成员名 buffer
} Screen;
// 定义一个包含 2 个 Screen 对象的数组
Screen myScreens[TM1640_TOTAL_DIGITS] = {
  { 0xC0, 4, DIS_NORMAL },   // 第一个屏幕：从 0xC0 开始，共 4 位，正常模式
  { 0xC8, 4, DIS_NORMAL },   // 第二个屏幕：从 0xC8 开始，共 4 位，闪烁模式
  { 0xC4, 1, DIS_COLORLED }  //三色led  1位 一定是 DIS_NOMODE 这个的反转在 获取颜色的方法中TM1640ColorLed
};
int screenCount = 3;                         // 当前有效屏幕数量
extern SystemData current;                   //【PWM 记录】当前所有的状态
extern volatile SettingMode curSettingMode;  //EC 11 设置模式
extern volatile bool isNeedSave;
// ----------------------------------------
// 【字符与字模转换】
// ----------------------------------------
static const uint8_t DIGIT_MAP[10] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };

inline uint8_t charToSegment(char c) {
  if (c >= '0' && c <= '9') return DIGIT_MAP[c - '0'];
  switch (c) {
    case 'A': return 0x77; ;
    case 'B':
    case 'b': return 0x7C;
    case 'C':
    case '[': return 0x39;
    case 'c': return 0x58;
    case 'D':
    case 'd': return 0x5E;
    case 'E':
    case 'e': return 0x79;
    case 'F': return 0x71;
    case 'U':
    case 'u': return 0x3E;

    case 'R':
    case 'r': return 0x50;
    case ']': return 0x0F;
    case 'P':
    case 'p': return 0x73;
    case 'H': return 0x76;
    case 'L': return 0x38;
    case 'S': return 0x6D;
    case '-': return 0x40;
    case '=': return 0x48;
    case 'o': return 0x5C;
    default: return 0x00;
  }
}

// ----------------------------------------
// 【数码管设置】
// ----------------------------------------
// setScreen 方法：支持修改和增加
void setScreen(int index, uint8_t addr, uint8_t len, DisplayMode mode) {
  if (index >= 0 && index < screenCount) {
    // 情况1：索引在有效范围内 → 修改已有项
    myScreens[index].startAddr = addr;
    myScreens[index].digitCount = len;
    myScreens[index].mode = mode;
  } else if (index == screenCount && screenCount < TM1640_TOTAL_DIGITS) {
    // 情况2：索引正好等于当前数量 → 在末尾增加新项
    myScreens[screenCount].startAddr = addr;
    myScreens[screenCount].digitCount = len;
    myScreens[screenCount].mode = mode;
    screenCount++;  // 有效数量+1
  }
  // 情况3：index > screenCount 或 index >= TM1640_TOTAL_DIGITS → 忽略（错误索引）
}


// //仅仅处理 数字数码管的闪烁    【具体显示 由 showText  统一处理 】
void updateFlashMsg() {
  for (int i = 0; i < screenCount; i++) {
    if (myScreens[i].mode == DIS_FLASH) {
      for (int j = 0; j < myScreens[i].digitCount; j++) {
        uint8_t index = myScreens[i].startAddr - 0xC0 + j;
        text[index] = ((millis() / flashInterval) % 2 == 0) ? myScreens[i].buffer[j] : 0;
      }
    }
  }
}

//设置显示内容仅仅整理内容 【具体显示 由 showText  统一处理 】
// 1. 如果是彩灯模式 mode != DIS_COLORLED，单独处理
void setDisplay(int screenIndex, const char *inputStr, DisplayMode mode) {
  if (mode != DIS_COLORLED) {            //必须非三色LED
    myScreens[screenIndex].mode = mode;  // 更新当前屏幕的显示模式
    // 1. 清空当前屏幕的 buffer
    for (int i = 0; i < myScreens[screenIndex].digitCount; i++) {
      myScreens[screenIndex].buffer[i] = 0x00;
    }
    // 2. 解析输入字符串并处理小数点粘连
    uint8_t rawBuffer[32] = { 0 };
    int totalItems = 0;
    int strIdx = 0;

    while (inputStr[strIdx] != '\0' && totalItems < 32) {
      char c = inputStr[strIdx];
      uint8_t seg = charToSegment(c);

      // 如果后面跟了小数点，将当前段码最高位加上小数点标志 (0x80)
      if (inputStr[strIdx + 1] == '.') {
        seg |= 0x80;
        strIdx++;
      }
      rawBuffer[totalItems++] = seg;
      strIdx++;
    }
    // 3. 将解析出的字模居右对齐写入当前 Screen 的 buffer 中
    for (int i = 0; i < myScreens[screenIndex].digitCount; i++) {

      if (i < totalItems) {
        myScreens[screenIndex].buffer[i] = rawBuffer[i];
      }
      uint8_t index = myScreens[screenIndex].startAddr - 0xC0 + i;
      text[index] = myScreens[screenIndex].buffer[i];
    }
  }
}

/*---------------------------------------------------------
  【以下 是 TM1640 核心显示 公共方法 】
---------------------------------------------------------*/
// 底层时序（内部统一使用新的常量名）
static inline void TM1640_Start() {
  digitalWrite(TM1640_SCL_PIN, HIGH);
  digitalWrite(TM1640_DIN_PIN, HIGH);
  delayMicroseconds(2);
  digitalWrite(TM1640_DIN_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TM1640_SCL_PIN, LOW);
}

static inline void TM1640_Stop() {
  digitalWrite(TM1640_SCL_PIN, LOW);
  digitalWrite(TM1640_DIN_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TM1640_SCL_PIN, HIGH);
  delayMicroseconds(2);
  digitalWrite(TM1640_DIN_PIN, HIGH);
}

static inline void TM1640_WriteByte(uint8_t data = 0) {
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(TM1640_SCL_PIN, LOW);
    digitalWrite(TM1640_DIN_PIN, (data & 0x01) ? HIGH : LOW);
    delayMicroseconds(2);
    digitalWrite(TM1640_SCL_PIN, HIGH);
    delayMicroseconds(2);
    data >>= 1;
  }
  digitalWrite(TM1640_SCL_PIN, LOW);
}

// 发送单字节到指定硬件地址
inline void sendTM1640(uint8_t addr, uint8_t data) {
  TM1640_Start();
  TM1640_WriteByte(0x44);
  TM1640_Stop();

  TM1640_Start();
  TM1640_WriteByte(addr);
  TM1640_WriteByte(data);
  TM1640_Stop();
}
// ----------------------------------------
// 【彩色LED设置】更新显示颜色 仅仅保存要显示的颜色的值 或者不显示的值  【具体显示 由 showText  统一处理 】
// ----------------------------------------
void updateLedColor() {

  for (int j = 0; j < myScreens[2].digitCount; j++) {
    int index = myScreens[2].startAddr - 0xC0 + j;
    // 防止越界写入
    if (index < 0 || index >= TM1640_TOTAL_DIGITS) {
      continue;
    }
    text[index] = getColor();
  }
}
// ----------------------------------------
// 显示核心方法（通过对比 text 和 last 判断内容变化，再逐位刷新） 【 将显示内容推送到TM 1640进行显示 】
// ----------------------------------------
inline void showText() {
  // 1. 使用 memcmp 代替 strcmp，直接对比两个 uint8_t 数组的内容是否完全一致
  if (memcmp(text, last, sizeof(text)) != 0) {
    //显示内容逻辑
    //2 复制显示数据
    for (int i = 0; i < TM1640_TOTAL_DIGITS; i++) {
      if (text[i] != last[i]) {
        sendTM1640(0xC0 + i, text[i]);
      }
    }
    // 2. 使用 memcpy 代替 strcpy，完成 uint8_t 数组的状态同步
    memcpy(last, text, sizeof(text));
  }
}
// 清除显示（清空全部 16 位）
inline void clear() {
  for (int i = 0; i < TM1640_TOTAL_DIGITS; i++) {
    sendTM1640(0xC0 + i, 0x00);
  }
  last[0] = '\0';
}

// 清除指定地址开始的指定位数
inline void clear(uint8_t startAddr, int count = 1) {
  if (count <= 0) {
  } else {
    for (int i = 0; i < count; i++) {
      uint8_t targetAddr = startAddr + i;
      if (targetAddr <= 0xCF) {
        sendTM1640(targetAddr, 0x00);
      }
    }
    last[0] = '\0';
  }
}
extern SaveData saved;
extern String sub_str;  
extern String main_str;
//正常模式下 设置显示内容
inline void settingDisplayByNormalMode() {
  // 非设置模式下的 显示当前温度信息
  if (!isNeedSave && curSettingMode == NORMAL) {
    // 1. 设置主屏显示数字
    setDisplay(0, String(current.currentTemp).c_str(), DIS_NORMAL);
    // // 2. 设置副屏显示字符 
    setDisplay(1, (saved.isCoreTemp ? "C" : "H" + String(current.targetTemp, 3)).c_str(), DIS_NORMAL);
  }
  // 彩灯逻辑
  if (current.ErrCode != "") {
    // 3. 设置彩灯（假设传入颜色参数）
    // setDisplay(0, "RED", DIS_COLORLED);
    SetColorType(COLOR_RED_NONE);
    setDisplay(1, current.ErrCode.c_str(), DIS_FLASH);
  } else {
    if (current.workMode == WELDING) {  //分段 处理
      if (current.heateMode == FULL)      // 默认对应 0 全速  红黄
      {
        SetColorType(COLOR_RED_YELLOW);
      } else if (current.heateMode == LIMIT)  // 默认对应 1  线性  红蓝
      {
        SetColorType(COLOR_BLUE_RED);
      } else if (current.heateMode == PID)  //  // 默认对应 2   PID  红绿
      {
        if (current.targetTemp <= current.currentTemp) {
          SetColorType(COLOR_GREEN_NONE);
        } else {
          SetColorType(COLOR_GREEN_RED);
        }
      }
    } else if (current.workMode == LOWTEMP)  // 蓝色闪烁
    {
      SetColorType(COLOR_BLUE_NONE);
    } else if (current.workMode == SLEEP)  //  五彩闪烁
    {
      SetColorType(COLOR_7);
    }
  }
}


// ----------------------------------------
// 后台独立刷新任务（单任务统一驱动 16 位）
// ----------------------------------------
static void singleDisplayTaskCode(void *pvParameters) {
  for (;;) {
    settingDisplayByNormalMode();  //【1】显示内容主逻辑
    updateFlashMsg();              //【2】更新闪烁
                                   // 更新 3色LED
    updateLedColor();
    showText();  //【3】显示
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ----------------------------------------
// 初始化方法
// ----------------------------------------
inline void init_TM1640(const char *taskName = "displayTask", UBaseType_t priority = 1, int din = -1, int scl = -1) {
  if (din != -1) TM1640_DIN_PIN = din;
  if (scl != -1) TM1640_SCL_PIN = scl;

  pinMode(TM1640_SCL_PIN, OUTPUT);
  pinMode(TM1640_DIN_PIN, OUTPUT);

  digitalWrite(TM1640_DIN_PIN, LOW);
  TM1640_WriteByte(brightness);
  digitalWrite(TM1640_DIN_PIN, LOW);
  digitalWrite(TM1640_SCL_PIN, HIGH);
  digitalWrite(TM1640_DIN_PIN, HIGH);

  for (int i = 0; i < TM1640_TOTAL_DIGITS; i++) {
    sendTM1640(0xC0 + i, 0x00);
  }

  TM1640_Start();
  TM1640_WriteByte(0x88 | (brightness & 0x07));
  TM1640_Stop();

  last[0] = '\0';

  xTaskCreate(
    singleDisplayTaskCode,
    taskName,
    2048,
    NULL,
    priority,
    NULL);
}