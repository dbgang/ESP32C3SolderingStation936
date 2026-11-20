#pragma once
#include "FuncStandardPrint.h"
#include <Preferences.h>  // ESP32 专用非易失性存储库
#include "GlobalParam.h"
#include "TaskPWM.h"
Preferences prefs;  // 操作存储实例 创建 Preferences 实例
// --- 全局变量 ---

extern PIDConfig pidConfig;  //{PID}

extern SaveData needSave;
extern SaveData saved;

int autoSaveTime = 2000;  // 自动保存时间设置  和控制显示方式的时间
extern volatile bool isNeedSave;
extern volatile unsigned long lastOperTime;
extern String ErrCode;

// 定义传出数据的回调类型（参数用 float）
typedef void (*LoadSettingCallback)(SaveData saved);

// 保存回调指针
static LoadSettingCallback globalLoadSettingCallback = nullptr;

// 注册回调接口
inline void setLoadSettingCallback(LoadSettingCallback cb) {
  globalLoadSettingCallback = cb;
}

//************加载存储值************
void loadSettings() {
  // 以只读模式打开命名空间 "soldering"
  if (prefs.begin("soldering", true)) { 
    print(String(prefs.getFloat("targetT", 111)).c_str());
    // 第二个参数是默认值：如果 Flash 中没有保存过，则保持全局变量定义的初始值
    saved.WeldingTemp = prefs.getFloat("targetT", saved.WeldingTemp);
    saved.lowTemp = prefs.getFloat("lowT", saved.lowTemp);
    saved.tipOffset = prefs.getFloat("offsetT", saved.tipOffset);
    saved.isCoreTemp = prefs.getBool("showCore", saved.isCoreTemp);
    saved.sleepTemp = prefs.getBool("sleepTemp", saved.isCoreTemp); 
    prefs.end();
    print(">> 配置加载成功:");
    print("   Target: %.1f, Low: %.1f, Offset: %.1f, Mode: %s",
          saved.WeldingTemp, saved.lowTemp, saved.tipOffset, saved.isCoreTemp ? "CORE" : "TIP");
  } else {
    print(">> 未发现存储记录，使用默认配置。");
  }
  needSave = saved;
  //传出需要显示的目标温度值
  if (globalLoadSettingCallback) {
    globalLoadSettingCallback(saved);  // 把值传输出去！
  }
}

//************存储值************
void saveSettings() {
  // 调低温度后 解除保护
  if (saved.WeldingTemp - needSave.WeldingTemp > pidConfig.THRES_BOOST_APPROACH) {
    ErrCode = "";
    print(">>设置 StopHeate  = false");
  }
  // if (needSave.lowTemp != saved.lowTemp) {
  //   needSave.lowTemp = constrain(needSave.lowTemp, 0, saved.WeldingTemp - TemperatureDrop);  // 低温自动控制
  // }
  // if (needSave.WeldingTemp != saved.WeldingTemp) {
  //   needSave.WeldingTemp = constrain(needSave.WeldingTemp, 0, saved.lowTemp + TemperatureDrop);
  // }
  // 以读写模式打开（false）
  int i = 0;
  if (prefs.begin("soldering", false)) {
    // 只有当当前变量与存储的值不同时才 put
    // print("prefs : %.2f,needSave : %.2f ", prefs.getFloat("targetT"), needSave.WeldingTemp);
    if (prefs.getFloat("targetT") != needSave.WeldingTemp) {
      prefs.putFloat("targetT", needSave.WeldingTemp);
      i++;
    }
    if (prefs.getFloat("lowT") != needSave.lowTemp) {
      prefs.putFloat("lowT", needSave.lowTemp);
      i++;
    }
    if (prefs.getFloat("offsetT") != needSave.tipOffset) {
      prefs.putFloat("offsetT", needSave.tipOffset);
      i++;
    }
    if (prefs.getBool("showCore") != needSave.isCoreTemp) {
      prefs.putBool("showCore", needSave.isCoreTemp);
      i++;
    }
     if (prefs.getBool("sleepTemp") != needSave.sleepTemp) {
      prefs.putBool("sleepTemp", needSave.sleepTemp);
      i++;
    }
    prefs.end();
    if (i > 0) {
      loadSettings();
      print(">> 所有设置已保存至 Flash。");
    }

  } else {
    print(">> 保存失败：无法访问存储空间。");
  }
}


//*************按钮超出两秒无操作自动保存********************
// userTouched  按钮操作
void handleAutoSave() {
  if (isNeedSave && (millis() - lastOperTime > autoSaveTime)) {
    saveSettings();  // 保存 4 个核心变量到 Preferences
    isNeedSave = false;
    //  print("自动保存 & 返回工作模式 , StopHeate: %s\n", StopHeate ? "true" : "false");
  }
}

// ========================================
// 自动保存后台独立任务（FreeRTOS Task）
// ========================================
static void autoSaveTaskCode(void* pvParameters) {
  if (!prefs.begin("soldering", true)) {
    saveSettings();
  } else {
    prefs.end();
    loadSettings();
  }

  for (;;) {
    // 在后台周期性检测是否需要自动保存
    handleAutoSave();
    // 每 100ms 检查一次，既灵敏又不会占用过多 CPU
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ----------------------------------------
// 一键启动自动保存后台任务方法
// ----------------------------------------
inline void initAutoSave(const char* taskName = "AutoSaveTask", UBaseType_t priority = 1) {
  xTaskCreate(
    autoSaveTaskCode,  // 任务入口函数
    taskName,          // 任务名称
    4096,              // 栈大小 (Bytes)
    NULL,              // 传参
    priority,          // 优先级
    NULL               // 句柄
  );
}
