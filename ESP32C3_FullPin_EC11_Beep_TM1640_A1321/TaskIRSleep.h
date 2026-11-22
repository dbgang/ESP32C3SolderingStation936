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
//【实时状态相关】
extern volatile int beepStatus;  //0 切换模式  1 到达温度 或到达第二模式
extern volatile bool isInserted;
extern volatile unsigned long IRInTime;  //上次操作时间  millis()
extern volatile bool hasUsed;            // 是否使用过
extern volatile bool hasSetted;          //  是否设置过
//【MQTT 相关  】
extern MqttManager mqttManager;
String FanClientId = "AutoFan-48A86E936314";
String PUB_TOPIC = "device/" + FanClientId + "/Forward";  // Publish 订阅主题（若不想订阅可传 nullptr）
static char opermsg[256];                                 // 在栈上分配固定大小的缓冲区（或改为 static）
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
      hasSetted = true;
      if (irTriggered) {
        beepStatus = 0;
        isInserted = false;
        hasUsed = true;
        print("[IR] 烙铁取下 - Infrared beam restored");
        snprintf(opermsg, sizeof(opermsg), "Start");
        mqttManager.publish(opermsg, PUB_TOPIC);
        // TODO：在这里执行唤醒或禁止休眠操作
      } else {
        beepStatus = 0;
        isInserted = true;
        IRInTime = millis();
        print("[IR] 烙铁放回 - Infrared beam interrupted");
        snprintf(opermsg, sizeof(opermsg), "Stop");
        mqttManager.publish(opermsg, PUB_TOPIC);
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