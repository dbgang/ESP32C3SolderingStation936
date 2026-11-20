
#pragma once
#include "FuncStandardPrint.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <stdlib.h>
#include <math.h>

// ============================================================
// UDP 温度同步配置
// ============================================================

extern volatile double Max31855Temp;

static WiFiUDP udpTemp;

static constexpr uint16_t UDP_LOCAL_PORT = 8889;
static constexpr uint16_t UDP_REMOTE_PORT = 8888;

static const IPAddress THERMO_ESP_IP(192, 168, 1, 8);

// ============================================================
// UDP 温度接收与同步任务
// ============================================================

void udpTempTask(void* pvParameters) {

  // UDP 初始化状态
  bool isUdpStarted = false;

  // UDP 接收缓冲区
  char rxBuf[64] = { 0 };

  // 统计上一次 WiFi 状态
  bool wasConnected = false;

  for (;;) {
    // print(
    //   "[UDP任务] 剩余栈: %u bytes\n",
    //   uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));
    // ====================================================
    // 1. 检查 WiFi 连接状态
    // ====================================================

    bool connected = (WiFi.status() == WL_CONNECTED);

    // WiFi 断开
    if (!connected) {

      if (isUdpStarted) {

        udpTemp.stop();

        isUdpStarted = false;

        print("[UDP温度同步] WiFi断开，UDP已关闭");
      }

      wasConnected = false;

      vTaskDelay(pdMS_TO_TICKS(500));

      continue;
    }

    // ====================================================
    // 2. WiFi 刚刚恢复连接，初始化 UDP
    // ====================================================

    if (!isUdpStarted || !wasConnected) {

      udpTemp.stop();

      if (!udpTemp.begin(UDP_LOCAL_PORT)) {

        print("[UDP温度同步] UDP启动失败");

        isUdpStarted = false;

        vTaskDelay(pdMS_TO_TICKS(1000));

        continue;
      }

      WiFi.setSleep(false);

      isUdpStarted = true;
      wasConnected = true;

      print("[UDP温度同步] 服务启动成功");
    }

    // ====================================================
    // 3. 接收上一轮返回的温度
    // ====================================================

    int packetSize = udpTemp.parsePacket();

    if (packetSize > 0) {

      IPAddress remoteIP = udpTemp.remoteIP();

      // 限制接收长度，防止缓冲区溢出
      int len = udpTemp.read(rxBuf, sizeof(rxBuf) - 1);

      if (len > 0 && remoteIP == THERMO_ESP_IP) {

        rxBuf[len] = '\0';

        char* endPtr = nullptr;

        double temp = strtod(rxBuf, &endPtr);

        // 检查数据是否合法
        if (endPtr != rxBuf && *endPtr == '\0' && isfinite(temp) && temp >= -200.0 && temp <= 1372.0) {

          Max31855Temp = temp;
        }
      }
    }

    // ====================================================
    // 4. 发送温度请求
    // ====================================================

    if (udpTemp.beginPacket(THERMO_ESP_IP, UDP_REMOTE_PORT)) {

      udpTemp.print("GET_TEMP");

      udpTemp.endPacket();
    }

    // ====================================================
    // 5. 任务延时
    // ====================================================

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ============================================================
// 初始化 UDP 温度同步任务
// ============================================================

void initUdpTempTask() {

  BaseType_t result = xTaskCreate(
    udpTempTask,
    "UdpTempTask",
    4096,
    NULL,
    1,
    NULL);

  if (result != pdPASS) {
    print("[UDP温度同步] 任务创建失败");
  }
}
