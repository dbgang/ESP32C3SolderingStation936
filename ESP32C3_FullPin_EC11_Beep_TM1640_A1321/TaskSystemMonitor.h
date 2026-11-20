
#pragma once

#include <Arduino.h>
#include <string.h>
#include <inttypes.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_heap_caps.h>
#include <esp_timer.h>

// ============================================================
// ESP32-C3 完整系统资源诊断模块
//
// 功能：
// 1. CPU 总使用率、空闲率
// 2. 每个任务 CPU 运行时间占比
// 3. Heap 总内存、已用、剩余、历史最低剩余
// 4. 最大连续空闲内存
// 5. 内存碎片化估算
// 6. PSRAM 监控（有 PSRAM 时自动显示）
// 7. FreeRTOS 全部任务状态、优先级
// 8. 每个任务剩余栈空间
// 9. 任务栈不足预警
// 10. 内存不足预警
// 11. 系统运行时间
//
// 监控周期：5 秒
//
// 注意：
// CPU 及任务运行时间统计需要开启 Runtime Stats。
// 任务列表需要开启 configUSE_TRACE_FACILITY。
// ESP32-C3 为单核 CPU。
// ============================================================

class SystemMonitor {

private:

  // ==========================================================
  // 基础配置
  // ==========================================================

  static constexpr uint32_t LOG_INTERVAL_MS = 5000;

  // 最多记录的任务数量
  static constexpr uint32_t MAX_TASKS = 32;

  // 内存警告阈值
  static constexpr uint32_t WARN_FREE_HEAP = 20 * 1024;

  // 最大连续内存警告阈值
  static constexpr uint32_t WARN_LARGEST_BLOCK = 8 * 1024;

  // 任务剩余栈空间警告阈值（字节）
  static constexpr uint32_t WARN_STACK_BYTES = 256;

  // ==========================================================
  // 运行状态
  // ==========================================================

  uint32_t lastLogTime = 0;

  uint64_t totalSamples = 0;

  bool cpuInitialized = false;

  uint32_t lastTotalRuntime = 0;

  uint32_t lastIdleRuntime = 0;

  // ==========================================================
  // Runtime Stats 功能检测
  // ==========================================================

#if defined(configGENERATE_RUN_TIME_STATS) && \
    (configGENERATE_RUN_TIME_STATS == 1) && \
    defined(configUSE_TRACE_FACILITY) && \
    (configUSE_TRACE_FACILITY == 1) && \
    defined(portGET_RUN_TIME_COUNTER_VALUE)

  static constexpr bool CPU_STATS_ENABLED = true;

#else

  static constexpr bool CPU_STATS_ENABLED = false;

#endif

  // ==========================================================
  // FreeRTOS 任务信息
  // ==========================================================

#if defined(configUSE_TRACE_FACILITY) && \
    (configUSE_TRACE_FACILITY == 1)

  // 静态任务信息数组，避免动态内存申请
  TaskStatus_t taskStatus[MAX_TASKS];

  // 上一次采样的任务运行时间
  struct TaskRuntimeSnapshot {
    TaskHandle_t handle;
    uint32_t runtime;
  };

  TaskRuntimeSnapshot previousTasks[MAX_TASKS] = {};

  UBaseType_t previousTaskCount = 0;

#endif

  // ==========================================================
  // 任务状态转换
  // ==========================================================

  const char* getTaskStateName(eTaskState state) {

    switch (state) {

      case eRunning:
        return "Running";

      case eReady:
        return "Ready";

      case eBlocked:
        return "Blocked";

      case eSuspended:
        return "Suspend";

      case eDeleted:
        return "Deleted";

      default:
        return "Unknown";
    }
  }

  // ==========================================================
  // 查找上一次任务运行时间
  // ==========================================================

#if defined(configUSE_TRACE_FACILITY) && \
    (configUSE_TRACE_FACILITY == 1) && \
    defined(configGENERATE_RUN_TIME_STATS) && \
    (configGENERATE_RUN_TIME_STATS == 1)

  uint32_t getPreviousRuntime(TaskHandle_t handle) {

    for (UBaseType_t i = 0; i < previousTaskCount; i++) {

      if (previousTasks[i].handle == handle) {
        return previousTasks[i].runtime;
      }
    }

    return 0;
  }

#endif

  // ==========================================================
  // 系统运行时间
  // ==========================================================

  void printUptime() {

    uint64_t uptimeSeconds = millis() / 1000ULL;

    uint32_t days = uptimeSeconds / 86400ULL;

    uint32_t hours = (uptimeSeconds % 86400ULL) / 3600ULL;

    uint32_t minutes = (uptimeSeconds % 3600ULL) / 60ULL;

    uint32_t seconds = uptimeSeconds % 60ULL;

    Serial.printf(
      "[SYS] Uptime:%lu days %02lu:%02lu:%02lu\n",
      (unsigned long)days,
      (unsigned long)hours,
      (unsigned long)minutes,
      (unsigned long)seconds
    );
  }

public:

  // ==========================================================
  // 初始化
  // ==========================================================

  void begin() {

    lastLogTime = millis();

    totalSamples = 0;

    cpuInitialized = false;

    lastTotalRuntime = 0;

    lastIdleRuntime = 0;

#if defined(configUSE_TRACE_FACILITY) && \
    (configUSE_TRACE_FACILITY == 1)

    memset(previousTasks, 0, sizeof(previousTasks));

    previousTaskCount = 0;

#endif

    Serial.println();
    Serial.println("====================================");
    Serial.println("[SYS] SystemMonitor initialized");
    Serial.printf("[SYS] CPU Stats: %s\n",
      CPU_STATS_ENABLED ? "Enabled" : "Disabled");

    Serial.println("[SYS] Monitor Interval: 5000 ms");
    Serial.println("====================================");
  }

  // ==========================================================
  // 系统资源监控
  //
  // 建议在低优先级任务或 loop() 中调用
  // 不要放在 PID、PWM、ADC 等高实时性任务中
  // ==========================================================

  void update() {

    uint32_t now = millis();

    if ((uint32_t)(now - lastLogTime) < LOG_INTERVAL_MS) {
      return;
    }

    uint32_t elapsed = now - lastLogTime;

    lastLogTime = now;

    totalSamples++;

    // ========================================================
    // 1. Heap 内存监控
    // ========================================================

    uint32_t totalHeap = ESP.getHeapSize();

    uint32_t freeHeap = ESP.getFreeHeap();

    uint32_t minFreeHeap = ESP.getMinFreeHeap();

    uint32_t usedHeap = totalHeap >= freeHeap
      ? totalHeap - freeHeap
      : 0;

    float memoryUsage = totalHeap > 0
      ? (float)usedHeap * 100.0f / totalHeap
      : 0.0f;

    float memoryFreeRate = totalHeap > 0
      ? (float)freeHeap * 100.0f / totalHeap
      : 0.0f;

    // 最大连续空闲内存
    uint32_t largestBlock =
      heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

    // 内存碎片化估算
    float fragmentation = 0.0f;

    if (freeHeap > 0 && largestBlock <= freeHeap) {

      fragmentation =
        100.0f * (1.0f - (float)largestBlock / freeHeap);
    }

    // ========================================================
    // 2. PSRAM 监控
    // ========================================================

    uint32_t psramTotal =
      heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

    uint32_t psramFree =
      heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    uint32_t psramUsed = psramTotal >= psramFree
      ? psramTotal - psramFree
      : 0;

    // ========================================================
    // 3. CPU 监控
    // ========================================================

    float cpuUsage = -1.0f;

    float cpuIdle = -1.0f;

    uint32_t totalDelta = 0;

    uint32_t idleDelta = 0;

#if defined(configGENERATE_RUN_TIME_STATS) && \
    (configGENERATE_RUN_TIME_STATS == 1) && \
    defined(configUSE_TRACE_FACILITY) && \
    (configUSE_TRACE_FACILITY == 1) && \
    defined(portGET_RUN_TIME_COUNTER_VALUE)

    uint32_t currentTotalRuntime =
      (uint32_t)portGET_RUN_TIME_COUNTER_VALUE();

    // ========================================================
    // 4. 获取全部任务信息
    // ========================================================

    UBaseType_t taskCount = uxTaskGetSystemState(
      taskStatus,
      MAX_TASKS,
      NULL
    );

    uint32_t currentIdleRuntime = 0;

    // 查找 ESP32-C3 Idle 任务
    for (UBaseType_t i = 0; i < taskCount; i++) {

      if (strncmp(taskStatus[i].pcTaskName, "IDLE", 4) == 0) {

        currentIdleRuntime =
          (uint32_t)taskStatus[i].ulRunTimeCounter;

        break;
      }
    }

    // ========================================================
    // 5. 计算 CPU 使用率
    // ========================================================

    if (cpuInitialized) {

      // 使用无符号减法处理 32 位计数器回绕
      totalDelta = currentTotalRuntime - lastTotalRuntime;

      idleDelta = currentIdleRuntime - lastIdleRuntime;

      if (totalDelta > 0 && idleDelta <= totalDelta) {

        cpuIdle =
          (float)idleDelta * 100.0f / totalDelta;

        cpuUsage = 100.0f - cpuIdle;
      }
    }

    lastTotalRuntime = currentTotalRuntime;

    lastIdleRuntime = currentIdleRuntime;

    cpuInitialized = true;

#endif

    // ========================================================
    // 6. 输出系统信息
    // ========================================================

    Serial.println();

    Serial.println("========== SYSTEM DIAGNOSTIC ==========");

    // --------------------------------------------------------
    // CPU
    // --------------------------------------------------------

    if (cpuUsage >= 0.0f) {

      Serial.printf(
        "[CPU] Usage: %.1f%% | Idle: %.1f%%\n",
        cpuUsage,
        cpuIdle
      );

    } else {

      Serial.println("[CPU] N/A (Runtime Stats unavailable)");
    }

    // --------------------------------------------------------
    // Heap
    // --------------------------------------------------------

    Serial.printf(
      "[MEM] Total:%u Free:%u Used:%u\n",
      (unsigned int)totalHeap,
      (unsigned int)freeHeap,
      (unsigned int)usedHeap
    );

    Serial.printf(
      "[MEM] Usage:%.1f%% FreeRate:%.1f%%\n",
      memoryUsage,
      memoryFreeRate
    );

    Serial.printf(
      "[MEM] MinFree:%u LargestBlock:%u\n",
      (unsigned int)minFreeHeap,
      (unsigned int)largestBlock
    );

    Serial.printf(
      "[MEM] Fragmentation:%.1f%%\n",
      fragmentation
    );

    // --------------------------------------------------------
    // PSRAM
    // --------------------------------------------------------

    if (psramTotal > 0) {

      Serial.printf(
        "[PSRAM] Total:%u Free:%u Used:%u\n",
        (unsigned int)psramTotal,
        (unsigned int)psramFree,
        (unsigned int)psramUsed
      );

    } else {

      Serial.println("[PSRAM] Not available");
    }

    // --------------------------------------------------------
    // 系统运行时间
    // --------------------------------------------------------

    printUptime();

    Serial.printf(
      "[SYS] Samples:%llu Interval:%u ms\n",
      (unsigned long long)totalSamples,
      (unsigned int)elapsed
    );

    // ========================================================
    // 7. 内存异常预警
    // ========================================================

    bool memoryWarning = false;

    if (freeHeap < WARN_FREE_HEAP) {

      Serial.printf(
        "[WARN] LOW HEAP! Free:%u B\n",
        (unsigned int)freeHeap
      );

      memoryWarning = true;
    }

    if (largestBlock < WARN_LARGEST_BLOCK) {

      Serial.printf(
        "[WARN] LOW LARGEST BLOCK! %u B\n",
        (unsigned int)largestBlock
      );

      memoryWarning = true;
    }

    if (memoryWarning) {

      Serial.println("[WARN] Check dynamic memory allocation!");
    }

    // ========================================================
    // 8. FreeRTOS 任务诊断
    // ========================================================

#if defined(configUSE_TRACE_FACILITY) && \
    (configUSE_TRACE_FACILITY == 1)

    // 如果没有开启 Runtime Stats，这里仍然可以获取任务状态
#if !(defined(configGENERATE_RUN_TIME_STATS) && \
      (configGENERATE_RUN_TIME_STATS == 1) && \
      defined(portGET_RUN_TIME_COUNTER_VALUE))

    UBaseType_t taskCount = uxTaskGetSystemState(
      taskStatus,
      MAX_TASKS,
      NULL
    );

#endif

    UBaseType_t totalTasks = uxTaskGetNumberOfTasks();

    Serial.printf(
      "\n[TASK] Total:%u Showing:%u\n",
      (unsigned int)totalTasks,
      (unsigned int)taskCount
    );

    if (totalTasks > MAX_TASKS) {

      Serial.printf(
        "[WARN] Task list limited to %u tasks!\n",
        (unsigned int)MAX_TASKS
      );
    }

    Serial.println(
      "Name             State      Priority StackFree CPU"
    );

    bool stackWarning = false;

    // --------------------------------------------------------
    // 遍历所有任务
    // --------------------------------------------------------

    for (UBaseType_t i = 0; i < taskCount; i++) {

      // ESP-IDF FreeRTOS 的 HighWaterMark 单位是字节
      uint32_t stackFreeBytes =
        (uint32_t)taskStatus[i].usStackHighWaterMark;

      // ------------------------------------------------------
      // 计算任务 CPU 占比
      // ------------------------------------------------------

      float taskCpu = -1.0f;

#if defined(configGENERATE_RUN_TIME_STATS) && \
    (configGENERATE_RUN_TIME_STATS == 1) && \
    defined(portGET_RUN_TIME_COUNTER_VALUE)

      if (totalDelta > 0) {

        uint32_t currentRuntime =
          (uint32_t)taskStatus[i].ulRunTimeCounter;

        uint32_t previousRuntime =
          getPreviousRuntime(taskStatus[i].xHandle);

        // 任务运行时间增量
        uint32_t taskDelta = currentRuntime - previousRuntime;

        taskCpu =
          (float)taskDelta * 100.0f / totalDelta;
      }

#endif

      // ------------------------------------------------------
      // 输出任务信息
      // ------------------------------------------------------

      if (taskCpu >= 0.0f) {

        Serial.printf(
          "%-16s %-10s %-8u %4u B %5.1f%%\n",
          taskStatus[i].pcTaskName,
          getTaskStateName(taskStatus[i].eCurrentState),
          (unsigned int)taskStatus[i].uxCurrentPriority,
          (unsigned int)stackFreeBytes,
          taskCpu
        );

      } else {

        Serial.printf(
          "%-16s %-10s %-8u %4u B N/A\n",
          taskStatus[i].pcTaskName,
          getTaskStateName(taskStatus[i].eCurrentState),
          (unsigned int)taskStatus[i].uxCurrentPriority,
          (unsigned int)stackFreeBytes
        );
      }

      // ------------------------------------------------------
      // 任务栈空间预警
      // ------------------------------------------------------

      if (stackFreeBytes < WARN_STACK_BYTES) {

        Serial.printf(
          "[WARN] LOW STACK! Task:%s Free:%u B\n",
          taskStatus[i].pcTaskName,
          (unsigned int)stackFreeBytes
        );

        stackWarning = true;
      }
    }

    if (stackWarning) {

      Serial.println(
        "[WARN] Increase task stack size or check recursion!"
      );
    }

    // --------------------------------------------------------
    // 保存本次任务运行时间，供下一次计算 CPU 占比
    // --------------------------------------------------------

#if defined(configGENERATE_RUN_TIME_STATS) && \
    (configGENERATE_RUN_TIME_STATS == 1) && \
    defined(portGET_RUN_TIME_COUNTER_VALUE)

    previousTaskCount = taskCount;

    for (UBaseType_t i = 0; i < taskCount; i++) {

      previousTasks[i].handle = taskStatus[i].xHandle;

      previousTasks[i].runtime =
        (uint32_t)taskStatus[i].ulRunTimeCounter;
    }

#endif

#else

    Serial.println(
      "[TASK] N/A (configUSE_TRACE_FACILITY disabled)"
    );

#endif

    Serial.println("=======================================");
  }

};

// ============================================================
// 全局系统监控对象
// ============================================================

SystemMonitor systemMonitor;
