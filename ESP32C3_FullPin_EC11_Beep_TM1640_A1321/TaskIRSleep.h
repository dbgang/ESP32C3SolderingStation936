#pragma once
#include "FuncStandardPrint.h"
#include <Arduino.h>
// ============================================================
// 红外对射休眠控制 (FreeRTOS Task 版本)
// GPIO13：PC817 光耦输出
// ============================================================

#define IR_SENSOR_PIN 13
// PC817 输出为开集电极：
// LOW  = 光耦导通
// HIGH = 光耦截止
#define IR_TRIGGER_LEVEL LOW

extern volatile bool needBeep;
extern volatile bool isInserted;
extern volatile unsigned long IRInTime;  //上次操作时间  millis()
extern volatile uint IRInTimes;          // 插入次数
// 任务句柄
TaskHandle_t IrSensorTaskHandle = NULL;

// 轮询周期 (ms)
const unsigned long IR_CHECK_INTERVAL_MS = 500;

// ============================================================
// 红外对射状态读取
// ============================================================
bool readIrSensor() {
  return digitalRead(IR_SENSOR_PIN) == IR_TRIGGER_LEVEL;
}

// ============================================================
// 红外对射独立任务入口函数
// ============================================================
void irSensorTaskCode(void* pvParameters) {
  // 初始化当前状态
  bool irTriggered = (digitalRead(IR_SENSOR_PIN) == IR_TRIGGER_LEVEL);
  bool lastIrTriggered = irTriggered;


  print("Infrared sensor task started");
  print("Initial state: ");
  print(irTriggered ? "TRIGGERED" : "NORMAL");

  while (1) {
    // 读取当前状态
    irTriggered = readIrSensor();

    // 只在状态发生变化时执行
    if (irTriggered != lastIrTriggered) {
      lastIrTriggered = irTriggered;

      if (irTriggered) {
        needBeep = true;
        isInserted = false;
        print("[IR] 烙铁取下 - Infrared beam restored");
        // TODO：在这里执行唤醒或禁止休眠操作
      } else {
        needBeep = true; 
        isInserted = true;
        IRInTime = millis();
        IRInTimes++;
        print("[IR] 烙铁放回 - Infrared beam interrupted");
        // TODO：在这里执行允许休眠操作
      }
    }

    // 周期性延时 500ms（释放 CPU 给其他任务）
    vTaskDelay(pdMS_TO_TICKS(IR_CHECK_INTERVAL_MS));
  }
}

// ============================================================
// 初始化并启动红外对射任务
// ============================================================
void initIrSensorTask() {
  // 初始化引脚，使用内部上拉
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);

  // 创建独立 Task
  // 堆栈大小 2048 字节，优先级设为 2（中等偏下），运行在核心 1
  BaseType_t result = xTaskCreate(
    irSensorTaskCode,    // 任务入口函数
    "IrSensorTask",      // 任务名称
    4096,                // 堆栈大小 (bytes)
    NULL,                // 传递参数
    2,                   // 任务优先级
    &IrSensorTaskHandle  // 任务句柄
  );
  if (result == pdPASS) {
    print("IR Sensor Task created successfully!");
  } else {
    print("Failed to create IR Sensor Task!");
  }
}