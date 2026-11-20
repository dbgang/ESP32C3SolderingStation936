#pragma once
#include <Arduino.h>
#include "FuncStandardPrint.h"
#include "GlobalParam.h"
#define BUZZER_PIN 10  // 定义蜂鸣器引脚
// ==========================================
// 1. 声音配置与上下文结构体定义
// ==========================================
struct BeepConfig {
  int freq;      // 频率 (Hz)
  int timeBeep;  // 单次发声时间 (ms)
  int repeat;    // 重复次数
  int timeMute;  // 两次发声之间的静音时间 (ms)
  int priority;  // 优先级 (100:即时响应, 90:告警, 80:状态提示, 70:自动保存)
};

// 运行时的内部状态机上下文
struct BuzzerContext {
  BeepConfig activeConfig;  // 当前正在播放的声音配置
  int currentRepeat;        // 当前已完成的重复次数
  int elapsedTicks;         // 当前状态已持续的时间片计数 (按 5ms 累加)
  bool isBeeping;           // 当前是在发声状态还是静音状态
  bool isPlaying;           // 是否有声音正在播放
  uint8_t pin;
  TaskHandle_t taskHandle;
};

// 封装私有静态状态实体，避免多文件链接冲突
inline BuzzerContext& getBuzzerCtx() {
  static BuzzerContext ctx = { { 0, 0, 0, 0, 0 }, 0, 0, false, false, 0, NULL };
  return ctx;
}

// ==================== 声音预设库 ====================
const BeepConfig B_ROT_PLUS = { 3000, 10, 1, 0, 100 };        // 叮 (高速)
const BeepConfig B_ROT_MINUS = { 2000, 10, 1, 0, 100 };       // 嗒 (低速)
const BeepConfig B_SET_ENTER = { 2700, 40, 1, 0, 100 };       // 叮! (40ms)
const BeepConfig B_SET_EXIT = { 2700, 200, 1, 0, 100 };       // 哔—— (200ms)
const BeepConfig B_SET_LONG_ENTER = { 2400, 80, 2, 50, 95 };  // 中长按：嘟-嘟
const BeepConfig B_TOGGLE_MODE = { 3300, 60, 3, 30, 98 };     // 超长按：叮-叮-叮
const BeepConfig B_AUTO_EXIT = { 2700, 30, 2, 40, 70 };       // 滴-滴
const BeepConfig B_REACHED = { 2700, 40, 3, 40, 70 };         // 滴滴滴!
const BeepConfig B_TONE_HIGH = { 3000, 40, 1, 0, 80 };        //唤醒
const BeepConfig B_TONE_HIGH_2 = { 1500, 100, 1, 0, 80 };     //低温
const BeepConfig B_TONE_LOW = { 1500, 40, 1, 0, 80 };         //唤醒:
const BeepConfig B_SLEEP_LONG = { 1200, 400, 1, 0, 80 };      // 嘟—— (400ms)
const BeepConfig B_FAST_TICK = { 2000, 100, 1, 0, 90 };       // 开机
const BeepConfig ALERT_SOUND = { 3000, 100, 2, 50, 99 };      // 红灯告警

// ==========================================
// 2. 核心控制函数：停止与即时打断 (适配 Core 3.x)
// ==========================================
inline void stopBeep() {
  BuzzerContext& ctx = getBuzzerCtx();

  if (ctx.pin != 0) {
    // Core 3.x 中通过向引脚写 0 占空比来关闭 PWM
    ledcWrite(ctx.pin, 0);
  }

  ctx.isPlaying = false;
  ctx.isBeeping = false;
  ctx.currentRepeat = 0;
  ctx.elapsedTicks = 0;
  ctx.activeConfig.priority = 0;
}


inline void playBeep(const BeepConfig& config) {
  BuzzerContext& ctx = getBuzzerCtx();
  // 如果当前正在播放更高优先级的声音，忽略新请求
  if (ctx.isPlaying && config.priority < ctx.activeConfig.priority) {
    return;
  }
  // 强行打断当前声音，防止拖尾或音调卡在前一个声音上
  stopBeep();
  // 加载新配置并开始
  ctx.activeConfig = config;
  ctx.currentRepeat = 0;
  ctx.elapsedTicks = 0;
  ctx.isBeeping = true;
  ctx.isPlaying = true;
  if (ctx.pin != 0) {
    // Core 3.x 动态修改频率并输出声音
    ledcChangeFrequency(ctx.pin, config.freq, 8);  // 动态改变频率
    ledcWrite(ctx.pin, 128);                       // 50% 占空比发声
  }
}

// 通过编号触发声音的便利接口
inline void playBeepById(int id) {
  switch (id) {
    case 1: playBeep(B_ROT_PLUS); break;
    case 2: playBeep(B_ROT_MINUS); break;
    case 3: playBeep(B_SET_ENTER); break;
    case 4: playBeep(B_SET_EXIT); break;
    case 5: playBeep(B_SET_LONG_ENTER); break;
    case 6: playBeep(B_TOGGLE_MODE); break;
    case 7: playBeep(B_AUTO_EXIT); break;
    case 8: playBeep(B_REACHED); break;
    case 9: playBeep(B_FAST_TICK); break;     //开机
    case 10: playBeep(ALERT_SOUND); break;    //告警
    case 11: playBeep(B_TONE_HIGH); break;    //唤醒
    case 12: playBeep(B_TONE_HIGH_2); break;  //低温
    case 14: playBeep(B_SLEEP_LONG); break;   // 嘟—— (400ms)
    default: break;
  }
}

// ==========================================
// 3. 动态蜂鸣器后台任务 (基于固定 5ms 时间片状态机)
// ==========================================
inline void vBuzzerTask(void* pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(10);  // 固定时间片：5ms

  while (1) {
    BuzzerContext& ctx = getBuzzerCtx();

    if (ctx.isPlaying) {
      ctx.elapsedTicks += 5;  // 每个时间片推进 5ms

      if (ctx.isBeeping) {
        // 正在发声阶段，检查是否达到了单次发声时长
        if (ctx.elapsedTicks >= ctx.activeConfig.timeBeep) {
          if (ctx.pin != 0) {
            ledcWrite(ctx.pin, 0);  // 关闭蜂鸣器
          }
          ctx.elapsedTicks = 0;
          ctx.isBeeping = false;
          ctx.currentRepeat++;

          // 检查是否所有重复次数都已播完
          if (ctx.currentRepeat >= ctx.activeConfig.repeat) {
            ctx.isPlaying = false;  // 播放结束
          }
        }
      } else {
        // 处于静音等待阶段（用于多音节之间的间隔）
        if (ctx.elapsedTicks >= ctx.activeConfig.timeMute) {
          ctx.elapsedTicks = 0;
          if (ctx.currentRepeat < ctx.activeConfig.repeat) {
            if (ctx.pin != 0) {
              ledcWrite(ctx.pin, 128);  // 开始下一次发声
            }
            ctx.isBeeping = true;
          }
        }
      }
    }

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

// ==========================================
// 4. 初始化接口 (适配 Core 3.x 的 ledcAttach)
// ==========================================
inline void initBuzzerSystem() {
  BuzzerContext& ctx = getBuzzerCtx();
  ctx.pin = BUZZER_PIN;

  // Core 3.x 标准初始化：直接把引脚、初始频率、分辨率绑定在一起
  ledcAttach(ctx.pin, 4000, 8);
  ledcWrite(ctx.pin, 0);

  // 创建独立任务，分配在核心 0
  xTaskCreatePinnedToCore(
    vBuzzerTask,
    "BuzzerTask",
    1024,
    NULL,
    1,
    &ctx.taskHandle,
    0);
}