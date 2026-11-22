// 【 增加声音识别  顺时针 逆时针 点击 短按住 长按住】
#include <Arduino.h>
#include "GlobalParam.h"  //引用了EnumAndStruct
#include "GlobalFunc.h"   //引用了EnumAndStruct
#include "FuncStandardPrint.h"
#include "Func18B20.h"
#include "FuncADC.h"
#include <EnumAndStruct.h>
#include "TaskConfigServer.h"
#include "TaskEC11.h"
#include "TaskBuzzer.h"
#include "TaskTm1640.h"
#include "TM1640ColorLed.h"
#include "TaskAutoSave.h"

#include "MqttManager.h"
#include "TaskPWM.h"
#include "TaskIRSleep.h"

#include "UdpTemp.h"
#include "TaskSystemMonitor.h"


/*********【全局即使回传的变量信息】*********************/

float sub_number = 0.0f;   //副屏显示
float main_number = 0.0f;  //主屏显示
String sub_str = "";       //当前或设置中的值 错误设备状态
String main_str = "";      //目标或原始值  错误代码
String ErrCode = "";       //错误代码


// 定义全局变量实体 旋钮
volatile int8_t encoderDelta = 0;
volatile uint8_t lastEncState = 0;
volatile int8_t subPos = 0;
volatile uint8_t btnEvent = 0;
volatile uint16_t beepTargetCount = 1;
volatile uint16_t currentCount = 0;
volatile unsigned long setPwmTime = 0;  // 宣告變數，務必用 unsigned long

// 【交互控制变量】
volatile SettingMode curSettingMode = NORMAL;  //EC 11 设置模式
volatile bool isNeedSave = false;              //需要保存
volatile unsigned long lastOperTime = 0;       //上次操作时间  millis()
volatile int beepStatus = 0;
volatile bool isInserted;             //【IR】是否休眠
volatile unsigned long IRInTime = 0;  //【IR】 插入时间
volatile bool hasUsed = false;        // 开机后是否使用过
 volatile bool hasSetted = false;            // 是否设置过 
float temp_min = 0;
float temp_max = 600;
//保存数据
SaveData needSave;
SaveData saved;
SystemData current;   //【系统当前状态】
PIDConfig pidConfig;  //系统设置参数
//【mqtt wifi  参数设置 】
// 实例化管理器
MqttManager mqttManager;
String funcName = "SolderingStation936";
// 定义配置参数
const int RESET_PIN = 0;                    // 比如用 GPIO 0 (Boot按键) 作为重置配网引脚
const char* MQTT_SERVER = "192.168.1.251";  // MQTT 服务器 IP
const uint16_t MQTT_PORT = 1883;            // 端口
String SUB_TOPIC;                           // 订阅主题（若不想订阅可传 nullptr）
// 接收到 MQTT 消息时的回调函数
void onMqttMessageReceived(String topic, String payload) {
  print("[收到消息] 主题: %s | 内容: %s\n", topic.c_str(), payload.c_str());
  if (payload == "Stop") {
    print(" 执行MQTT命令Stop");
  }
  if (payload == "Start") {
    print(" 执行MQTT命令Start");
  }
  // 如果返回的是 JSON 格式，也可以在这里直接进行解析
}


//定义控制句柄
TaskHandle_t TaskEncoderHandle = NULL;
TaskHandle_t TaskButtonHandle = NULL;
TaskHandle_t TaskBuzzerHandle = NULL;

#define TM1640_DIN 18
#define TM1640_SCL 19

int Addr_main = 0xC0;
int Addr_sub = 0xC8;
int Addr_led = 0xC4;

String floatToStringWithPrefix(float number, String prefix = "", int decimalPlaces = 3) {
  String result = prefix + String(number, decimalPlaces);
  return result;
}


// 主程序的接收函数（对应你刚才提到的 LoadMySetting）  设置目标值  所以 只设置 副屏
void LoadMySetting(SaveData saved) {
  curSettingMode = NORMAL;
  sub_number = saved.WeldingTemp;
  sub_str = floatToStringWithPrefix(sub_number, saved.isCoreTemp ? "C" : "H");
  playBeepById(3);
  main_number = sub_number;
  main_str = floatToStringWithPrefix(main_number, "");
  setDisplay(1, sub_str.c_str(), DIS_NORMAL);
  setDisplay(0, main_str.c_str(), DIS_NORMAL);
  print("主程序收到了新值: %.2f", sub_number);
}

void changeMainDis(float value, SettingMode mode, DisplayMode* dismode) {
  switch (curSettingMode) {
    case NORMAL:  // 非设置模式 / 实时温度  // 显示实时温度，前缀为空或"温度:"
      sub_number = saved.WeldingTemp;
      sub_str = floatToStringWithPrefix(sub_number, saved.isCoreTemp ? "C" : "H");
      needSave.WeldingTemp += value;
      if (needSave.WeldingTemp < temp_min) {
        needSave.WeldingTemp -= value;
      }
      main_number = needSave.WeldingTemp;
      main_str = floatToStringWithPrefix(main_number);
      *dismode = DIS_NORMAL;
      print(" needSave.WeldingTemp : %.2f", needSave.WeldingTemp);
      break;
    case WELDING_TEMP:  // 设置焊接温度
      sub_number = saved.WeldingTemp;
      sub_str = floatToStringWithPrefix(sub_number, saved.isCoreTemp ? "C" : "H");
      needSave.WeldingTemp += value;
      //不低于 0  不大于 600
      if (needSave.WeldingTemp < temp_min || needSave.WeldingTemp > temp_max) {
        needSave.WeldingTemp -= value;
      }
      if (needSave.WeldingTemp < needSave.lowTemp) {
        needSave.lowTemp = needSave.WeldingTemp;  // 低于低温 调整低温
      }
      main_number = needSave.WeldingTemp;
      main_str = floatToStringWithPrefix(main_number);
      print(" needSave.WeldingTemp : %.2f", needSave.WeldingTemp);
      break;
    case LOW_TEMP:  // 设置低温
      sub_number = saved.lowTemp;
      sub_str = floatToStringWithPrefix(sub_number, "L");
      needSave.lowTemp += value;
      //不小于 0 并且 不大于 WeldingTemp
      if (needSave.lowTemp < 0 || needSave.WeldingTemp > temp_max) {
        needSave.lowTemp -= value;
      }
      if (needSave.WeldingTemp < needSave.lowTemp) {
        needSave.WeldingTemp = needSave.lowTemp;  //超过高温  调整高温
      }
      main_number = needSave.lowTemp;
      main_str = floatToStringWithPrefix(main_number);
      break;
    case OFFSET:  // 设置偏移
      sub_number = saved.tipOffset;
      sub_str = floatToStringWithPrefix(sub_number, "F");
      needSave.tipOffset += value;

      main_number = needSave.tipOffset;
      main_str = floatToStringWithPrefix(main_number);
      break;
    default:
      // 未知模式处理
      main_str = "----";
      setDisplay(0, main_str.c_str(), DIS_NORMAL);
      sub_str = "Error";
      break;
  }
}
// 1. 编写回调处理函数
void handleEC11Event(EC11Event event, float value, SettingMode mode) {
  lastOperTime = millis();
  if (!isNeedSave) {  //设置 开始设置了
    isNeedSave = true; 
  } 
    hasSetted = true;
  DisplayMode dismode = DIS_FLASH;
  switch (event) {
    case EVENT_CW:
      print("主程序收到：顺时针旋转");
      // 可以在这里根据当前模式增加温度或调整变量
      changeMainDis(value, mode, &dismode);
      break;
    case EVENT_CCW:
      print("主程序收到：逆时针旋转");
      changeMainDis(value, mode, &dismode);
      break;
    case EVENT_CLICK:
      print("主程序收到：单击确定");
      if (curSettingMode == NORMAL) {
        dismode = DIS_NORMAL;
        isNeedSave = false;
        saveSettings();
      } else {
        changeMainDis(0, mode, &dismode);
      }
      break;
    case EVENT_DOUBLE_CLICK:
      print("主程序收到：双击 ");
      changeMainDis(0, mode, &dismode);
      break;
    case EVENT_LONG:  //低温设置
      print("主程序收到：中长按");
      changeMainDis(0, mode, &dismode);
      break;
    case EVENT_SUPER_LONG:  //切换 显示模式 芯温度或头温度
      print("主程序收到：超长按 ");
      isNeedSave = false;
      needSave.isCoreTemp = !needSave.isCoreTemp;
      if (needSave.isCoreTemp) {
        needSave.WeldingTemp += needSave.tipOffset;
      } else {
        needSave.WeldingTemp = max(0.0f, needSave.WeldingTemp - needSave.tipOffset);  //不得小于 0
      }
      saveSettings();
      dismode = DIS_NORMAL;
      changeMainDis(0, mode, &dismode);
      break;
    default:
      break;
  }
  setDisplay(0, main_str.c_str(), dismode);
  setDisplay(1, sub_str.c_str(), DIS_NORMAL);
}


void setup() {
  Serial.begin(115200);
  systemMonitor.begin();
  //【10 MQTT 通讯】
  char uuid[40];                 // 2. 在本地定义缓冲区
  fillUUID(uuid, sizeof(uuid));  // 3. 传进去，安全又清晰
                                 // 3. 在 setup 内部安全地拼接订阅主题
  SUB_TOPIC = "device/" + clientId + "/command";
  // 初始化 MQTT 管理器（内部会自动处理 WiFiManager 配网与重启）
  mqttManager.initMQtt(SUB_TOPIC.c_str(), onMqttMessageReceived);
  // initUdpTempTask();
  /******【注册回调】*************/
  setEC11Callback(handleEC11Event);
  setLoadSettingCallback(LoadMySetting);
  delay(1000);
  //【1 环境温度】
  init18B20();
  //【6 ADC 和 实际温度读数】
  initADC();
  //【2 蜂鸣同步播报】
  initBuzzerSystem();  // 3. 初始化 EC11 编码器与按键模块并启动后台任务
  //【3 旋钮伺服】
  initEC11System();
  //【4 数码管显示】
  init_TM1640();
  //【5 自动保存】
  initAutoSave();
  //【7 PWM 控制】
  initPWMAndTask();
  //【8 网页设置参数 服务启动】
  initWebServerTask();
  //【9 红外休眠】
  initIrSensorTask();
  // 开机提示
  playBeepById(9);
  print("[系统]: 模块化解耦架构启动成功！");
}
//设置显示数据 和播放 音效  模式切换
WorkMode lastMode = Mode_None;

void loop() {
  if( current.targetTemp -current.currentTemp >  -2 && current.targetTemp -current.currentTemp < 2 ){
    hasSetted =false;
  }
  // systemMonitor.update();
  if (current.ErrCode != "") {
    //  print("A0");
    playBeepById(10);  //B_REACHED

  } else { 
    if (current.workMode == WELDING) {
       if (beepStatus == 0 && lastMode != current.workMode) {  //B_REACHED 切模式 告警
        beepStatus++;
        playBeepById(8);
      } else if ((beepStatus == 1  ) && current.heateMode == PID && current.currentTemp >= current.targetTemp) {  //【 到达目的温度提示】
        beepStatus++;
        playBeepById(8);
      }
      lastMode = WELDING;
    } else if (beepStatus == 0 && current.workMode == LOWTEMP && lastMode != current.workMode) { //切低温模式 
      beepStatus++;
      lastMode = LOWTEMP;
      playBeepById(12);  //B_TONE_HIGH_2
    } else if (beepStatus == 1 && current.workMode == SLEEP && lastMode != current.workMode) { //切 睡眠模式
      beepStatus++;
      lastMode = SLEEP;
      playBeepById(14);  //B_TONE_HIGH_2
    }
  }
  delay(500);
  // vTaskDelete(NULL);
  // print("温度%0.2f", Temp18B20);
  // i++;
  // //模拟数码管变化
  // // strcpy(mainScreen.text, i);
  // char text[16];
  // sprintf(text, "%d", i);  // 直接格式化写入，省去 strcpy
  // // delay(100);
  // // 1. 正常调用第一个屏幕（比如显示 "123."，4位数码管，居右对齐）
  // setDisplay(0, text, DIS_NORMAL);
  // // 2. 调用第二个屏幕（比如显示 "-5.6"，4位数码管，闪烁模式）
  // setDisplay(1, "-5.6A", DIS_FLASH);
}
