#pragma once
#include <EnumAndStruct.h>

//*********【3 使用变量定义】*************
TempParams coreHeating = {
  .adc_ref = 11393.0f,  // 起始参考 ADC 值
  .temp_ref = 115.01f,  // 对应参考点的线性拟合基准温度（实测 110.8°C）
  .cold_base = 25.0f,   // 采集本组数据时 temp18B20 的环境基准读数（可按实际室温微调）
  .slope = 0.05656f     // 【正斜率】每增加 1 个 ADC 计数，温度上升约 0.07691°C
};
TempParams tempParams = {
  .adc_ref = 11393.0f,  // ADC 参考值
  .temp_ref = 115.01f,  // ADC=11393 时的拟合温度
  .cold_base = 25.0f,   // 冷端参考温度，单位℃
  .slope = 0.05656f     // ADC 每增加 1，对应温度变化量
};

//TempParams coreHeating = { 9956.0, 24.0, 24.0, 0.02181 };
TempParams coreCooling = coreHeating;
TempParams tipHeating = coreHeating;  // 头-升温
TempParams tipCooling = coreHeating;  // 头-降温

// 预设不同的档位
TipProfile profiles[] = {
  { "Large Knife", 2.5f, 0.1f, 12.0f, 255, 15 },  // 大刀头：高积分、全功率
  { "Small Point", 1.2f, 0.03f, 4.0f, 160, 5 }    // 小尖头：低积分、限功率
};
