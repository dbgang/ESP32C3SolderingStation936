#pragma once
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include "UUIDHeler.h"
#include "GlobalParam.h"  
#include "FuncStandardPrint.h"
// 回调函数指针类型
typedef void (*MqttCallback)(String topic, String payload);

class MqttManager {
private:

  WiFiClient _espClient;
  PubSubClient _mqttClient;

  char _mqttServer[64];
  uint16_t _mqttPort;
  String _subTopic = "device/" + clientId + "/command";  //订阅主题;
  String _pubTopic = "device/" + clientId + "/status";
  int _resetPin;
  String _clientId;

  TaskHandle_t _mqttTaskHandle;
  static MqttManager* _instance;
  MqttCallback _userCallback;

  // 保存 MQTT 配置到 Preferences (EEPROM模拟)
  void saveMqttConfig(const char* server, uint16_t port, const char* topic) {
    Preferences prefs;
    prefs.begin("mqtt_cfg", false);
    prefs.putString("server", server);
    prefs.putUShort("port", port);
    prefs.putString("topic", topic);
    prefs.end();
  }

  // 从 Preferences 读取 MQTT 配置
  void loadMqttConfig(const char* defaultServer, uint16_t defaultPort, const char* defaultTopic) {
    Preferences prefs;
    prefs.begin("mqtt_cfg", true);
    String sServer = prefs.getString("server", defaultServer);
    uint16_t sPort = prefs.getUShort("port", defaultPort);
    String sTopic = prefs.getString("topic", defaultTopic ? defaultTopic : "");
    prefs.end();

    strlcpy(_mqttServer, sServer.c_str(), sizeof(_mqttServer));
    _mqttPort = sPort;
    _subTopic = sTopic;
  }


  // 清除网络配置和MQTT配置并重启
public:
  void resetSettingsAndRestart() {
    WiFiManager wm;
    wm.resetSettings();

    Preferences prefs;
    prefs.begin("mqtt_cfg", false);
    prefs.clear();
    prefs.end();

    print("[WiFiManager] 配置已全部清除，正在重启...");
    delay(1000);
    ESP.restart();
  }


  static void mqttTaskEntry(void* pvParameters) {
    MqttManager* manager = (MqttManager*)pvParameters;
    manager->mqttTaskLoop();
  }

  void mqttTaskLoop() {
    while (true) {
      if (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        continue;
      }

      if (!_mqttClient.connected()) {
        print("[MQTT] 正在连接到服务器:{%s :%s】", _mqttServer, String(_mqttPort));

        if (_mqttClient.connect(_clientId.c_str())) {
          print("[MQTT] 连接成功！");
          if (_subTopic.length() > 0) {
            _mqttClient.subscribe(_subTopic.c_str());
            print("[MQTT] 已订阅主题: ");
            print(_subTopic.c_str());
          }
        } else {
          print("[MQTT] 连接失败, rc=");
          print(String(_mqttClient.state()).c_str());
          print(" 5秒后重试...");
          vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
      } else {
        _mqttClient.loop();
      }

      if (_resetPin >= 0 && digitalRead(_resetPin) == LOW) {
        delay(50);
        if (digitalRead(_resetPin) == LOW) {
          print("[WiFiManager] 检测到重置引脚触发，正在清除配置并重启...");
          WiFiManager wm;
          wm.resetSettings();
          // 同时清除保存的 MQTT 配置
          Preferences prefs;
          prefs.begin("mqtt_cfg", false);
          prefs.clear();
          prefs.end();
          ESP.restart();
        }
      }

      vTaskDelay(50 / portTICK_PERIOD_MS);
    }
  }

  static void handleMqttMessage(char* topic, byte* payload, unsigned int length) {
    if (_instance && _instance->_userCallback) {
      String t = String(topic);
      String p = "";
      for (unsigned int i = 0; i < length; i++) {
        p += (char)payload[i];
      }
      _instance->_userCallback(t, p);
    }
  }

public:
  MqttManager()
    : _mqttClient(_espClient) {
    _instance = this;
    _resetPin = -1;
    _mqttPort = 1883;
    memset(_mqttServer, 0, sizeof(_mqttServer));
    _subTopic = "";
    _userCallback = nullptr;
  }

  void initMQtt(
    const char* defaultTopic,                     // 1. 必须输入
    MqttCallback callback,                        // 2. 必须输入
    const char* defaultServer = "192.168.1.251",  // 3. 可选（有默认值）
    uint16_t defaultPort = 1883,                  // 4. 可选（有默认值）
    int resetPin = -1                             // 5. 可选（有默认值）
  ) {
    _resetPin = resetPin;
    _userCallback = callback;
    _clientId = clientId;

    // 4. 赋值服务器地址（把字符串复制到字符数组里）
    //strncpy(_mqttServer, defaultServer, sizeof(_mqttServer) - 1);
    strncpy(_mqttServer, defaultServer, sizeof(_mqttServer));
    _mqttServer[sizeof(_mqttServer) - 1] = '\0';  // 确保安全结尾
    // 5. 赋值端口
    _mqttPort = defaultPort;
    if (_resetPin >= 0) {
      pinMode(_resetPin, INPUT_PULLUP);
    }

    // 1. 先从 Flash 加载已保存的 MQTT 设置
    loadMqttConfig(defaultServer, defaultPort, defaultTopic);

    // --- WiFiManager 配网及自定义参数逻辑 ---
    WiFiManager wm;

    // 定义网页输入框组件 (id, 标签提示文字, 默认值, 最大长度)
    char portStr[8];
    snprintf(portStr, sizeof(portStr), "%u", _mqttPort);

    WiFiManagerParameter custom_mqtt_server("server", "MQTT 服务器 IP/域名", _mqttServer, 63);
    WiFiManagerParameter custom_mqtt_port("port", "MQTT 端口", portStr, 6);
    WiFiManagerParameter custom_mqtt_topic("topic", "MQTT 订阅主题", _subTopic.c_str(), 63);

    // 将参数加入到 WiFiManager 中
    wm.addParameter(&custom_mqtt_server);
    wm.addParameter(&custom_mqtt_port);
    wm.addParameter(&custom_mqtt_topic);

    print("[WiFiManager] 正在检查网络配置...");
    bool res = wm.autoConnect(_clientId.c_str());

    if (!res) {
      print("[WiFiManager] 配网超时或失败，即将重启...");
      ESP.restart();
    } else {
      print("[WiFi] WiFi 连接成功！");

      // 配网页面提交后，把用户输入的新 MQTT 参数读取并保存下来
      strlcpy(_mqttServer, custom_mqtt_server.getValue(), sizeof(_mqttServer));
      _mqttPort = String(custom_mqtt_port.getValue()).toInt();
      _subTopic = String(custom_mqtt_topic.getValue());

      if (_mqttPort == 0) _mqttPort = 1883;  // 防错处理

      saveMqttConfig(_mqttServer, _mqttPort, _subTopic.c_str());
      print("[WiFiManager] 已更新并保存 MQTT 配置！");
    }

    // --- MQTT 初始化 ---
    _mqttClient.setServer(_mqttServer, _mqttPort);
    _mqttClient.setCallback(this->handleMqttMessage);
    _mqttClient.setBufferSize(1024);

    // --- 创建后台任务 ---
    xTaskCreate(
      this->mqttTaskEntry,
      "MqttTask",
      4096,
      this,
      1,
      &_mqttTaskHandle);
  }

  // 手动调起网页配网及修改配置
  void startConfigPortal() {
    WiFiManager wm;

    // 重新加载当前值填充输入框
    char portStr[8];
    snprintf(portStr, sizeof(portStr), "%u", _mqttPort);

    WiFiManagerParameter custom_mqtt_server("server", "MQTT 服务器 IP/域名", _mqttServer, 63);
    WiFiManagerParameter custom_mqtt_port("port", "MQTT 端口", portStr, 6);
    WiFiManagerParameter custom_mqtt_topic("topic", "MQTT 订阅主题", _subTopic.c_str(), 63);



    wm.addParameter(&custom_mqtt_server);
    wm.addParameter(&custom_mqtt_port);
    wm.addParameter(&custom_mqtt_topic);

    print("[WiFiManager] 正在手动启动配置后台...");

    if (!wm.startConfigPortal(_clientId.c_str())) {
      print("[WiFiManager] 配置超时或失败，准备重启...");
      delay(2000);
      ESP.restart();
    } else {
      // 保存用户在网页上修改后的新 MQTT 参数
      strlcpy(_mqttServer, custom_mqtt_server.getValue(), sizeof(_mqttServer));
      _mqttPort = String(custom_mqtt_port.getValue()).toInt();
      _subTopic = String(custom_mqtt_topic.getValue());
      if (_mqttPort == 0) _mqttPort = 1883;

      saveMqttConfig(_mqttServer, _mqttPort, _subTopic.c_str());
      print("[WiFiManager] 新配置已保存，正在重启...");
      delay(1000);
      ESP.restart();
    }
  }

  //提交/发布信息
  //提交/发布信息
  bool publish(String msg, String topic = "") {
    if (topic == "") {
      topic = _pubTopic;  // 假设 _pubTopic 是 String 类型
    }
    // 修正：打印传入的消息 msg（或者如果你有定义 jsonPayload 变量则保留）
    print(msg);
    if (_mqttClient.connected()) {
      // 修正：msg 需要用 .c_str() 转换为 const char*
      return _mqttClient.publish(topic.c_str(), msg.c_str());  // 发布 MQTT
    }
    return false;
  }

  // 优化：将 char* &msg 改为 const char* msg
  bool publishChars(const char* msg, String topic = "") {
    if (topic == "") {
      topic = _pubTopic;  // 假设 _pubTopic 是 String 类型
    }

    // 假设这是你的打印函数，或者改为 print(msg);
    print(msg);

    if (_mqttClient.connected()) {
      // 修正：msg 本身就是指针，直接传入，去掉 .c_str()
      return _mqttClient.publish(topic.c_str(), msg);
    }
    return false;
  }
  //====================== 【省电】 =========================

  volatile int mqttUsedCount = 0;  //wifi MQTT 使用数 可能会在程序流程之外被改变，不要对它进行优化

  /* ================= 辅助函数：连接/重连 MQTT ================= */
  void reconnectMQTT() {
    while (!_mqttClient.connected()) {
      print("正在连接 MQTT 服务器...");
      if (_mqttClient.connect(_clientId.c_str())) {
        publish("Raio开启成功");
      } else {
        print("连接失败, rc=%d. 5秒后重试...\n", _mqttClient.state());
        vTaskDelay(pdMS_TO_TICKS(5000));
      }
    }
  }

  void OpenRadio(const char* pos) {
    WiFiManager wm;
    int i = 0;
    print("【%s】需要 ", pos);
    print("启动[WiFiTask] 正在启动 WiFiManager 自动配网...");
    wm.setTimeout(180);
    bool res = wm.autoConnect(_clientId.c_str());
    if (!res) {
      print("[WiFiTask] 配网超时或连接失败...");
    } else {
      print("[WiFiTask] WiFi 连接成功！");
    }
    _mqttClient.setServer(_mqttServer, _mqttPort);
    while (i++ < 5) {
      if (WiFi.status() != WL_CONNECTED) {
        print("[WiFiTask] WiFi 断开，尝试重连...");
        WiFi.mode(WIFI_STA);
        // Serial.begin(115200);    // 再开串口
        WiFi.reconnect();
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
      }
      if (!_mqttClient.connected()) {
        reconnectMQTT();
      }
      _mqttClient.loop();
    }
    mqttUsedCount++;
  }

  //节电关闭Radio设备
  void CloseRadio() {
    mqttUsedCount--;
    print("【关闭Radio】 值: %d  \n", mqttUsedCount);
    if (mqttUsedCount < 1) {
      _mqttClient.loop();
      publish("【关闭Radio】 值: " + String(mqttUsedCount) + "  \n");
      // 3. 【核心修复】给 MQTT 协议栈时间发送数据
      // 循环调用 loop() 确保数据被推送到网卡 确保发送完成 vTaskDelay(pdMS_TO_TICKS(1000));
      for (int i = 0; i < 10; i++) {
        _mqttClient.loop();
        vTaskDelay(pdMS_TO_TICKS(100));  // 总共 1 秒的发送窗口
      }
      //关闭WiFi
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      //  Serial.end();              // 先关串口
      // setCpuFrequencyMhz(80);   // 降频（可选）  setCpuFrequencyMhz(240);  // 恢复 240MHz 降频后，硬件 UART 的波特率会跟着变，因为时钟源变了。// 降到 80MHz 后，实际波特率变成约 38400
      //关闭蓝牙
      btStop();
    }
  }
};

MqttManager* MqttManager::_instance = nullptr;

IPAddress ip = WiFi.localIP();
