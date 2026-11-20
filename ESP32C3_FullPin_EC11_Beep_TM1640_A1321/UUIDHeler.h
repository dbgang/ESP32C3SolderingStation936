#pragma once
//******* ****[启动次数计数]******************/
#include <Preferences.h>    
uint32_t bootCount;
String func = "936SolderingStation-";
//初始化启动计数
// 初始化启动计数
inline void initFlashCount() {
    Preferences prefs; // 本地声明 Preferences 对象，避免外部混淆
    prefs.begin("system", false);
    bootCount = prefs.getUInt("boot", 0); // 修正：将 prefsUUID 改为 prefs
    bootCount++;
    prefs.putUInt("boot", bootCount);
    prefs.end();
}

// 创建UUID 【设备ID + 启动次数 + 开机时长】
inline void fillUUID(char* buf, size_t len) {
    uint64_t mac = ESP.getEfuseMac();
    uint32_t ms = millis();
    snprintf(buf, len, "%016llX-%08lu-%010lu", (unsigned long long)mac, bootCount, (unsigned long)ms);
}

// 获取 clientId
inline String initClientId() {
    uint64_t mac = ESP.getEfuseMac();
    char idBuf[32];
    snprintf(idBuf, sizeof(idBuf), "%04X%08X", (uint16_t)(mac >> 32), (uint32_t)mac);
    return String(idBuf);
}

static String clientId = func+  initClientId();  // 生成唯一的 Client ID