
#ifndef STANDARD_PRINT_H
#define STANDARD_PRINT_H

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

// ============================================================
// 标准打印模块
// 重要：底层必须直接调用 Serial.print()
// 不能再调用自定义 print()，否则会产生无限递归。
// ============================================================

// 格式化输出的底层实现
inline void standardPrintImpl(const char *format, va_list args) {
    // 使用局部缓冲区，避免多个 FreeRTOS 任务共享同一个 buf
    char buf[512] = {0};

    // 执行格式化
    int len = vsnprintf(buf, sizeof(buf), format, args);

    // 格式化失败时直接输出错误信息
    if (len < 0) {
        Serial.println("[ERROR] Log formatting failed");
        return;
    }

    // 没有内容则不输出
    if (len == 0) {
        return;
    }

    // 直接输出串口，禁止调用自定义 print()
    Serial.print(buf);
    Serial.print("\r\n");
}

// ============================================================
// 格式化打印
// 示例：print("温度：%.2f", temperature);
// ============================================================
inline void print(const char *format, ...) {
    va_list args;
    va_start(args, format);

    standardPrintImpl(format, args);

    va_end(args);
}

// ============================================================
// String 类型打印  适合动态字符串	非常适合	不适合
// 示例：print(String("Hello"));
// ============================================================
// inline void print(String msg) {  会消耗内存
//     Serial.print(msg);
//     Serial.print("\r\n");
// }
inline void print(const String &msg) {   // 适合动态字符串	非常适合  ；字符串存储	RAM	 
  Serial.println(msg);
}

inline void print(const __FlashStringHelper *msg) {    // 适合动态字符串	不适合   ；字符串存储 Flash
  Serial.println(msg);
}

// ============================================================
// 始终打印
// 保留原来的函数名称 Allwaysprint，兼容现有调用
// ============================================================
inline void Allwaysprint(const char *format, ...) {
    va_list args;
    va_start(args, format);

    standardPrintImpl(format, args);

    va_end(args);
}

// ============================================================
// 任务初始化状态
// ============================================================
inline void printInit(String taskName, String value, String func = "") {
    print("%s(%s):开启状态【%s】",
          func.c_str(),
          taskName.c_str(),
          value.c_str());
}

// ============================================================
// 任务运行状态
// ============================================================
inline void printTask(String taskName, String value, String func = "") {
    print("%s(%s):CurrentValue【%s】",
          func.c_str(),
          taskName.c_str(),
          value.c_str());
}

//【各个功能的核心扩展变量 】 
void printSystemLog(const SystemData &data) { 
  print("{  \"time\": %lu,  \"currentADC\": %.1f,  \"currentTemp\": %.1f,\"targetTemp\": %.1f, \"PWM\": %lu} ", millis(), data.currentADC, data.currentTemp, data.targetTemp,data.pwm);  
  
  // print("{\n"
  //       "  \"time\": %lu,\n"
  //       "  \"IRInTime\": %lu,\n"
  //       "  \"IRKeepInTime\": %lu,\n"
  //       "  \"isInserted\": %d,\n"
  //       "  \"StopHeate\": %d,\n"
  //       "  \"lowTemp\": %.1f,\n"
  //       "  \"sleepTemp\": %.1f,\n"
  //       "  \"targetTemp\": %.1f,\n"
  //       "  \"temp18B20\": %.1f,\n"
  //       "  \"orgADC\": %.1f,\n" 
  //       "  \"currentADC\": %.1f,\n"
  //       "  \"orgTemp\": %.1f,\n"
  //       "  \"currentTemp\": %.1f,\n"
  //       "  \"orgVolt\": %.6f,\n"
  //       "  \"currentVolt\": %.6f,\n"
  //       "  \"pwm\": %d,\n"
  //       "  \"heateMode\": %d,\n"
  //       "  \"workMode\": %d,\n"
  //       "  \"settingMode\": %d,\n"
  //       "  \"ErrCode\": \"%s\"\n"
  //       "} ",
  //       data.time,
  //       data.InSertTime,
  //       data.IRKeepInTime,
  //       data.isInserted? 1 : 0,
  //       data.StopHeate ? 1 : 0,
  //       data.lowTemp,
  //       data.sleepTemp,
  //       data.targetTemp,
  //       data.temp18B20,
  //       data.orgADC, 
  //       data.currentADC,
  //       data.orgTemp,
  //       data.currentTemp,
  //       data.orgVolt,
  //       data.currentVolt,
  //       data.pwm,
  //       (int)data.heateMode,
  //       (int)data.workMode,
  //       data.settingMode,
  //       data.ErrCode);
}


#endif // STANDARD_PRINT_H
