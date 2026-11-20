#pragma once
#include <Arduino.h>
#include "GlobalParam.h"  
#include "FuncStandardPrint.h"
// ----------------------------------------
// 物理寄存器值定义
// ----------------------------------------
#define L_OFF 0x00
#define L_RED 0x01
#define L_GREEN 0x02
#define L_YELLOW 0x03
#define L_BLUE 0x04
#define L_PURPLE 0x05
#define L_CYAN 0x06
#define L_WHITE 0x07
// ----------------------------------------
// 逻辑颜色 ID 枚举
// ----------------------------------------
enum ColorType {
  COLOR_NONE = 0,
  COLOR_RED = 1,
  COLOR_GREEN = 2,
  COLOR_YELLOW = 3,
  COLOR_BLUE = 4,
  COLOR_PURPLE = 5,
  COLOR_CYAN = 6,
  COLOR_WHITE = 7,

  // 呼吸/闪烁/灭灯组
  COLOR_RED_NONE = 11,
  COLOR_GREEN_NONE = 12,
  COLOR_YELLOW_NONE = 13,
  COLOR_BLUE_NONE = 14,
  COLOR_PURPLE_NONE = 15,
  COLOR_CYAN_NONE = 16,
  COLOR_WHITE_NONE = 17,

  // 双色交替组
  COLOR_RED_YELLOW = 20,
  COLOR_GREEN_YELLOW = 21,
  COLOR_GREEN_RED = 22,
  COLOR_GREEN_BLUE = 23,
  COLOR_BLUE_YELLOW = 24,
  COLOR_BLUE_RED = 25,

  // 七彩组
  COLOR_7 = 30
};

uint8_t currentColorType = COLOR_NONE;

// ----------------------------------------
// 设置当前颜色与动画状态（直接传参，无需传指针）
// ----------------------------------------
inline void SetColorType(uint8_t color) {
  currentColorType = color;
}
// 使用 unsigned long 匹配 millis() 的返回类型，可以写在全局或函数开头
unsigned long colorInterval = 400;  //变色 间隔 毫秒
// 这个需要定期调用 因为有变色
uint8_t getColor() {
  uint8_t displayColor = COLOR_NONE;
  switch (currentColorType) {
    // ---- 常亮纯色组 ----
    case COLOR_NONE:
      displayColor = 0;
      break;
    case COLOR_GREEN:
      displayColor = L_GREEN;
      break;
    case COLOR_BLUE:
      displayColor = L_BLUE;
      break;
    case COLOR_RED:
      displayColor = L_RED;
      break;
    case COLOR_YELLOW:
      displayColor = L_YELLOW;
      break;
    case COLOR_WHITE:
      displayColor = L_WHITE;
      break;
    case COLOR_PURPLE:
      displayColor = L_PURPLE;
      break;
    case COLOR_CYAN:
      displayColor = L_CYAN;
      break;

    // ---- 单色闪烁组 (亮灭交替) ----
    case COLOR_RED_NONE:
      displayColor = ((millis() / colorInterval) % 2 == 0) ? L_RED : 0;
      break;
    case COLOR_GREEN_NONE:
      displayColor = ((millis() / colorInterval) % 2 == 0) ? L_GREEN : 0;
      break;
    case COLOR_YELLOW_NONE:
      displayColor = ((millis() / colorInterval) % 2 == 0) ? L_YELLOW : 0;
      break;
    case COLOR_BLUE_NONE:
      displayColor = ((millis() / colorInterval) % 2 == 0) ? L_BLUE : 0;
      break;
    case COLOR_PURPLE_NONE:
      displayColor = ((millis() / colorInterval) % 2 == 0) ? L_PURPLE : 0;
      break;
    case COLOR_CYAN_NONE:
      displayColor = ((millis() / colorInterval) % 2 == 0) ? L_CYAN : 0;
      break;
    case COLOR_WHITE_NONE:
      displayColor = ((millis() / colorInterval) % 2 == 0) ? L_WHITE : 0;
      break;

    // ---- 双色交替组 ----
    case COLOR_RED_YELLOW:
      displayColor = ((millis() / colorInterval) % 2 == 0) ? L_RED : L_YELLOW;
      break;
    case COLOR_GREEN_YELLOW:
      displayColor = ((millis() / colorInterval) % 2 == 0) ? L_GREEN : L_YELLOW;
      break;
    case COLOR_GREEN_RED:
      displayColor = ((millis() / colorInterval) % 2 == 0) ? L_GREEN : L_RED;
      break;
    case COLOR_GREEN_BLUE:
      displayColor = ((millis() / colorInterval) % 2 == 0) ? L_GREEN : L_BLUE;
      break;
    case COLOR_BLUE_YELLOW:
      displayColor = ((millis() / colorInterval) % 2 == 0) ? L_BLUE : L_YELLOW;
      break;
    case COLOR_BLUE_RED:
      displayColor = ((millis() / colorInterval) % 2 == 0) ? L_BLUE : L_RED;
      break;

    // ---- 七彩循环组 ----
    case COLOR_7:
      {
        const uint8_t constrainbow[] = { L_RED, L_YELLOW, L_GREEN, L_CYAN, L_BLUE, L_PURPLE, L_WHITE };
        displayColor = constrainbow[(millis() / colorInterval) % 7];
        break;
      }

    default:
      displayColor = COLOR_NONE;
      break;
  }
  return displayColor;
}
