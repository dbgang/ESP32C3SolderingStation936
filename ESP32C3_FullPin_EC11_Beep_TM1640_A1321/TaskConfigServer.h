
#include <WebServer.h>
#include "EnumAndStruct.h"
#include <WiFi.h>       // ESP32 Wi-Fi 库，提供 WiFi 和 WL_CONNECTED
#include <math.h>
// ============================================================
// WebServer 配置
// ============================================================

// 创建监听 8080 端口的 WebServer 对象
WebServer server(8080);

// 后台 Web 任务句柄
TaskHandle_t WebServerTaskHandle = NULL;

// 外部 PID 配置对象
extern PIDConfig pidConfig;
extern SystemData current;

// // 当前温度
// current.currentTemp;

// // PWM 输出比例转换为百分比
// current.pwmRate * 100.0f;

// ============================================================
// 网页 HTML 模板
// 手机优先响应式布局，适配 6 英寸手机
// 每个输入框右侧提供 - / + 步进按钮
// ============================================================

const char* webPanelHtml = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">

<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">

<title>A1321 烙铁温控面板</title>

<style>

/* ============================================================
   页面整体布局
   ============================================================ */

* {
    box-sizing: border-box;
}

body {
    font-family: Arial, "Microsoft YaHei", sans-serif;
    margin: 0;
    padding: 12px;
    background: #f2f4f8;
    color: #333;
}

/* 主卡片 */

.card {
    width: 100%;
    max-width: 520px;
    margin: 0 auto;
    padding: 18px 15px;
    background: #ffffff;
    border-radius: 12px;
    box-shadow: 0 2px 10px rgba(0,0,0,0.08);
}

/* 标题 */

h2 {
    margin: 5px 0 20px;
    text-align: center;
    font-size: 21px;
    color: #222;
}

/* 参数分组标题 */

.group-title {
    margin-top: 22px;
    margin-bottom: 12px;
    padding: 8px 10px;
    background: #edf3ff;
    border-left: 4px solid #2878e8;
    border-radius: 4px;
    font-size: 15px;
    font-weight: bold;
    color: #2864bd;
}

/* 参数标签 */

label {
    display: block;
    margin-top: 14px;
    margin-bottom: 6px;
    font-size: 14px;
    font-weight: bold;
    color: #444;
}

/* ============================================================
   输入框与步进按钮
   ============================================================ */

.input-group {
    display: flex;
    width: 100%;
    height: 46px;
    gap: 6px;
}

/* 参数输入框 */

.input-group input {
    flex: 1;
    min-width: 0;
    height: 46px;
    padding: 0 10px;
    border: 1px solid #ccd2dc;
    border-radius: 6px;
    background: #fff;
    font-size: 17px;
    color: #222;
    outline: none;
}

/* 输入框获得焦点 */

.input-group input:focus {
    border-color: #2878e8;
    box-shadow: 0 0 0 2px rgba(40,120,232,0.12);
}

/* - / + 按钮 */

.step-btn {
    flex: 0 0 46px;
    width: 46px;
    height: 46px;
    border: none;
    border-radius: 7px;
    background: #e8edf5;
    color: #333;
    font-size: 25px;
    font-weight: bold;
    cursor: pointer;
    touch-action: manipulation;
    user-select: none;
}

/* 按钮点击效果 */

.step-btn:active {
    background: #cbd8ec;
    transform: scale(0.95);
}

/* 减少按钮 */

.step-btn.minus {
    color: #d9534f;
}

/* 增加按钮 */

.step-btn.plus {
    color: #218838;
}

/* ============================================================
   提交按钮
   ============================================================ */

.submit-btn {
    width: 100%;
    height: 50px;
    margin-top: 28px;
    border: none;
    border-radius: 8px;
    background: #28a745;
    color: white;
    font-size: 17px;
    font-weight: bold;
    cursor: pointer;
}

.submit-btn:active {
    background: #218838;
    transform: scale(0.98);
}

/* 页面底部说明 */

.footer {
    margin-top: 15px;
    text-align: center;
    font-size: 12px;
    color: #999;
}

/* ============================================================
   电脑屏幕适配
   ============================================================ */

@media (min-width: 600px) {

    body {
        padding: 25px;
    }

    .card {
        padding: 25px;
    }

    h2 {
        font-size: 23px;
    }
}
/* ============================================================
   实时温度 / PWM 曲线区域
   ============================================================ */

.chart-card {
    width: 100%;
    margin-bottom: 22px;
    padding: 12px 8px;
    background: #ffffff;
    border: 1px solid #e5e8ee;
    border-radius: 10px;
}

.chart-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 3px 5px 10px;
    font-size: 15px;
}

#chartStatus {
    font-size: 11px;
    color: #20a45a;
}

.chart-legend {
    display: flex;
    justify-content: center;
    gap: 25px;
    margin-bottom: 5px;
    font-size: 13px;
}

.temp-legend {
    color: #2685e8;
}

.pwm-legend {
    color: #20a45a;
}

#temperatureChart {
    display: block;
    width: 100%;
    height: 220px;
}

.chart-footer {
    display: flex;
    justify-content: space-between;
    padding: 5px;
    color: #999;
    font-size: 11px;
}
</style>



</head>

<body>

<div class="card">

<h2>A1321 温控参数调节</h2>
<!-- 实时曲线区域 -->
<div class="chart-card">

    <div class="chart-header">
        <strong>实时温度 / PWM 曲线</strong>
        <span id="chartStatus">等待数据...</span>
    </div>

    <div class="chart-legend">
        <span class="temp-legend">● 温度 ℃</span>
        <span class="pwm-legend">● PWM %</span>
    </div>

    <canvas id="temperatureChart"></canvas>

    <div class="chart-footer">
        <span>历史数据：最近 60 秒</span>
        <span>采样间隔：200ms</span>
    </div>

</div>
<form action="/update" method="POST">

<!-- ========================================================
     PID 参数
     ======================================================== -->

<div class="group-title">一、PID 控制参数</div>

<label>Kp（比例系数）</label>
<div class="input-group">
    <input id="kp" type="number" step="any" name="kp" class="param-input" value="%VAL_KP%">
    <button type="button" class="step-btn minus" onclick="changeValue('kp',-1)">−</button>
    <button type="button" class="step-btn plus" onclick="changeValue('kp',1)">+</button>
</div>

<label>Ki（积分系数）</label>
<div class="input-group">
    <input id="ki" type="number" step="any" name="ki" class="param-input" value="%VAL_KI%">
    <button type="button" class="step-btn minus" onclick="changeValue('ki',-0.1)">−</button>
    <button type="button" class="step-btn plus" onclick="changeValue('ki',0.1)">+</button>
</div>

<label>Kd（微分系数）</label>
<div class="input-group">
    <input id="kd" type="number" step="any" name="kd" class="param-input" value="%VAL_KD%">
    <button type="button" class="step-btn minus" onclick="changeValue('kd',-1)">−</button>
    <button type="button" class="step-btn plus" onclick="changeValue('kd',1)">+</button>
</div>


<!-- ========================================================
     温度模式切换阈值
     ======================================================== -->

<div class="group-title">二、温度模式切换阈值</div>

<label>全速冲刺阈值（THRES_BOOST_FULL）</label>
<div class="input-group">
    <input id="b_full" type="number" step="any" name="b_full" class="param-input" value="%VAL_BF%">
    <button type="button" class="step-btn minus" onclick="changeValue('b_full',-1)">−</button>
    <button type="button" class="step-btn plus" onclick="changeValue('b_full',1)">+</button>
</div>

<label>线性减速阈值（THRES_BOOST_APPROACH）</label>
<div class="input-group">
    <input id="b_app" type="number" step="any" name="b_app" class="param-input" value="%VAL_BA%">
    <button type="button" class="step-btn minus" onclick="changeValue('b_app',-1)">−</button>
    <button type="button" class="step-btn plus" onclick="changeValue('b_app',1)">+</button>
</div>

<label>停止加热阈值（THRES_HEATER_OFF）</label>
<div class="input-group">
    <input id="h_off" type="number" step="any" name="h_off" class="param-input" value="%VAL_HO%">
    <button type="button" class="step-btn minus" onclick="changeValue('h_off',-1)">−</button>
    <button type="button" class="step-btn plus" onclick="changeValue('h_off',1)">+</button>
</div>


<!-- ========================================================
     积分分离与限幅
     ======================================================== -->

<div class="group-title">三、积分分离与限幅</div>

<label>积分分离阈值（THRES_INTEGRAL_SPLIT）</label>
<div class="input-group">
    <input id="i_split" type="number" step="any" name="i_split" class="param-input" value="%VAL_IS%">
    <button type="button" class="step-btn minus" onclick="changeValue('i_split',-1)">−</button>
    <button type="button" class="step-btn plus" onclick="changeValue('i_split',1)">+</button>
</div>

<label>积分项最大正向限幅（INTEGRAL_MAX_LIMIT）</label>
<div class="input-group">
    <input id="i_max" type="number" step="any" name="i_max" class="param-input" value="%VAL_IMAX%">
    <button type="button" class="step-btn minus" onclick="changeValue('i_max',-1)">−</button>
    <button type="button" class="step-btn plus" onclick="changeValue('i_max',1)">+</button>
</div>

<label>积分项最大负向限幅（INTEGRAL_MIN_LIMIT）</label>
<div class="input-group">
    <input id="i_min" type="number" step="any" name="i_min" class="param-input" value="%VAL_IMIN%">
    <button type="button" class="step-btn minus" onclick="changeValue('i_min',-1)">−</button>
    <button type="button" class="step-btn plus" onclick="changeValue('i_min',1)">+</button>
</div>


<!-- ========================================================
     超温保护
     ======================================================== -->

<div class="group-title">四、超温保护</div>

<label>超温保护关闭阈值（OVERHEAT_PROTECT_OFF）</label>
<div class="input-group">
    <input id="o_off" type="number" step="any" name="o_off" class="param-input" value="%VAL_OO%">
    <button type="button" class="step-btn minus" onclick="changeValue('o_off',-1)">−</button>
    <button type="button" class="step-btn plus" onclick="changeValue('o_off',1)">+</button>
</div>


<!-- ========================================================
     提交按钮
     ======================================================== -->

<button type="submit" class="submit-btn">
    立即应用并保存参数
</button>

</form>

<div class="footer">
    A1321 Temperature Control System
</div>

</div>
<script>
/* ============================================================
   A1321 实时温度 / PWM 双 Y 轴曲线
   ============================================================ */

// 历史数据
const chartHistory = [];

// 最多保存 120 个采样点
// 500ms × 120 = 60 秒
const CHART_MAX_POINTS = 120;

// Canvas 对象
const chartCanvas = document.getElementById("temperatureChart");
const chartCtx = chartCanvas.getContext("2d");

// 曲线绘图区边距
const chartPadding = {
    left: 34,
    right: 34,
    top: 12,
    bottom: 12
};


/* ============================================================
   Canvas 分辨率适配
   兼容手机高分辨率屏幕
   ============================================================ */

function resizeTemperatureChart() {

    const rect = chartCanvas.getBoundingClientRect();

    const dpr = window.devicePixelRatio || 1;

    chartCanvas.width = rect.width * dpr;
    chartCanvas.height = rect.height * dpr;

    chartCtx.setTransform(dpr, 0, 0, dpr, 0, 0);

    drawTemperatureChart();
}


/* ============================================================
   绘制坐标轴、网格和双曲线
   左 Y 轴：0～500℃
   右 Y 轴：0～100%
   ============================================================ */

function drawTemperatureChart() {

    const ctx = chartCtx;

    const width = chartCanvas.clientWidth;
    const height = chartCanvas.clientHeight;

    if (width <= 0 || height <= 0) return;

    const left = chartPadding.left;
    const right = width - chartPadding.right;
    const top = chartPadding.top;
    const bottom = height - chartPadding.bottom;

    const plotWidth = right - left;
    const plotHeight = bottom - top;

    // 清空画布
    ctx.clearRect(0, 0, width, height);

    // 背景
    ctx.fillStyle = "#ffffff";
    ctx.fillRect(0, 0, width, height);

    // ========================================================
    // 绘制横向网格和左右 Y 轴刻度
    // ========================================================

    ctx.font = "10px Arial";
    ctx.textBaseline = "middle";

    for (let i = 0; i <= 5; i++) {

        const y = top + plotHeight * i / 5;

        // 横向网格
        ctx.beginPath();
        ctx.strokeStyle = "#e9edf2";
        ctx.lineWidth = 1;
        ctx.setLineDash([3, 3]);

        ctx.moveTo(left, y);
        ctx.lineTo(right, y);

        ctx.stroke();
        ctx.setLineDash([]);

        // 左侧温度刻度
        ctx.fillStyle = "#2685e8";
        ctx.textAlign = "right";

        ctx.fillText(
            String(500 - i * 100),
            left - 5,
            y
        );

        // 右侧 PWM 百分比刻度
        ctx.fillStyle = "#20a45a";
        ctx.textAlign = "left";

        ctx.fillText(
            String(100 - i * 20),
            right + 5,
            y
        );
    }

    // ========================================================
    // 坐标轴
    // ========================================================

    ctx.beginPath();
    ctx.strokeStyle = "#aab2bd";
    ctx.lineWidth = 1;

    ctx.moveTo(left, top);
    ctx.lineTo(left, bottom);
    ctx.lineTo(right, bottom);
    ctx.moveTo(right, top);
    ctx.lineTo(right, bottom);

    ctx.stroke();

    // ========================================================
    // 没有数据时不绘制曲线
    // ========================================================

   // if (chartHistory.length < 2) return;

    // 每个采样点之间的水平距离
    const step = plotWidth / (CHART_MAX_POINTS - 1);

    // ========================================================
    // 绘制温度曲线
    // ========================================================

    ctx.beginPath();
    ctx.strokeStyle = "#2685e8";
    ctx.lineWidth = 2;
    ctx.lineJoin = "round";
    ctx.lineCap = "round";

    chartHistory.forEach((item, index) => {

        // 最新数据固定在最右侧
        const x = right - (chartHistory.length - 1 - index) * step;

        // 温度映射到 0～500℃
        const temp = Math.max(0, Math.min(500, item.temperature));

        const y = bottom - temp / 500 * plotHeight;

        if (index === 0) {
            ctx.moveTo(x, y);
        } else {
            ctx.lineTo(x, y);
        }

    });

    ctx.stroke();

    // ========================================================
    // 绘制 PWM 曲线
    // ========================================================

    ctx.beginPath();
    ctx.strokeStyle = "#20a45a";
    ctx.lineWidth = 2;
    ctx.lineJoin = "round";
    ctx.lineCap = "round";

    chartHistory.forEach((item, index) => {

        const x = right - (chartHistory.length - 1 - index) * step;

        // PWM 映射到 0～100%
        const pwm = Math.max(0, Math.min(100, item.pwm));

        const y = bottom - pwm / 100 * plotHeight;

        if (index === 0) {
            ctx.moveTo(x, y);
        } else {
            ctx.lineTo(x, y);
        }

    });

    ctx.stroke();
}


/* ============================================================
   接收 ESP32 实时数据
   ============================================================ */

async function updateTemperatureChart() {

    try {

        const response = await fetch("/data", {
            cache: "no-store"
        });

        if (!response.ok) {
            throw new Error("HTTP " + response.status);
        }

        const data = await response.json();

        const temperature = Number(data.temperature);
        const pwm = Number(data.pwm);

        if (!Number.isFinite(temperature) ||
            !Number.isFinite(pwm)) {
            throw new Error("数据无效");
        }

        // 加入最新采样点
        chartHistory.push({
            temperature: temperature,
            pwm: pwm
        });

        // 超出容量后删除最旧的数据
        if (chartHistory.length > CHART_MAX_POINTS) {
            chartHistory.shift();
        }

        // 更新曲线
        drawTemperatureChart();

        document.getElementById("chartStatus").textContent = "● 实时连接";

    }
    catch (error) {

        document.getElementById("chartStatus").textContent = "连接异常";

        console.log("温度曲线数据获取失败:", error);

    }

}


/* ============================================================
   初始化曲线
   ============================================================ */

resizeTemperatureChart();

window.addEventListener("resize", resizeTemperatureChart);

// 每 200ms 获取一次数据
setInterval(updateTemperatureChart, 200);

// 页面加载后立即获取一次
updateTemperatureChart();
/* ============================================================
   参数步进控制
   每次点击增加或减少 1
   ============================================================ */

function changeValue(inputId, direction) {

    const input = document.getElementById(inputId);

    if (!input) return;

    // 获取当前输入值
    let value = parseFloat(input.value);

    // 如果输入为空或无效，则从 0 开始
    if (isNaN(value)) {
        value = 0;
    }

    // 每次步进 1
    value += direction;

    // 保留最多 4 位小数，避免浮点数精度问题
    value = Math.round(value * 10000) / 10000;

    // 更新输入框
    input.value = value;
}

/* ============================================================
   页面加载完成后初始化
   ============================================================ */

document.addEventListener("DOMContentLoaded", function() {

    // 允许通过键盘输入小数
    document.querySelectorAll(".param-input").forEach(function(input) {

        input.setAttribute("inputmode", "decimal");

    });

});

</script>
</body>
</html>
)rawliteral";


// ============================================================
// 注册网页路由
// ============================================================

void setupWebRoutes() {

  // --------------------------------------------------------
  // 1. 访问主页
  // --------------------------------------------------------

  server.on("/", HTTP_GET, []() {
    // 复制 HTML 模板
    String html = webPanelHtml;

    // 替换 PID 参数
    html.replace("%VAL_KP%", String(pidConfig.Kp, 4));
    html.replace("%VAL_KI%", String(pidConfig.Ki, 4));
    html.replace("%VAL_KD%", String(pidConfig.Kd, 4));

    // 替换温度模式参数
    html.replace("%VAL_BF%", String(pidConfig.THRES_BOOST_FULL, 4));
    html.replace("%VAL_BA%", String(pidConfig.THRES_BOOST_APPROACH, 4));
    html.replace("%VAL_HO%", String(pidConfig.THRES_HEATER_OFF, 4));

    // 替换积分控制参数
    html.replace("%VAL_IS%", String(pidConfig.THRES_INTEGRAL_SPLIT, 4));
    html.replace("%VAL_IMAX%", String(pidConfig.INTEGRAL_MAX_LIMIT, 4));
    html.replace("%VAL_IMIN%", String(pidConfig.INTEGRAL_MIN_LIMIT, 4));

    // 替换超温保护参数
    html.replace("%VAL_OO%", String(pidConfig.OVERHEAT_PROTECT_OFF, 4));

    // 返回网页
    server.send(200, "text/html; charset=utf-8", html);
  });


  // --------------------------------------------------------
  // 2. 接收参数提交
  // --------------------------------------------------------

  server.on("/update", HTTP_POST, []() {
    // PID 参数
    if (server.hasArg("kp")) pidConfig.Kp = server.arg("kp").toFloat();
    if (server.hasArg("ki")) pidConfig.Ki = server.arg("ki").toFloat();
    if (server.hasArg("kd")) pidConfig.Kd = server.arg("kd").toFloat();

    // 温度模式参数
    if (server.hasArg("b_full")) pidConfig.THRES_BOOST_FULL = server.arg("b_full").toFloat();
    if (server.hasArg("b_app")) pidConfig.THRES_BOOST_APPROACH = server.arg("b_app").toFloat();
    if (server.hasArg("h_off")) pidConfig.THRES_HEATER_OFF = server.arg("h_off").toFloat();

    // 积分控制参数
    if (server.hasArg("i_split")) pidConfig.THRES_INTEGRAL_SPLIT = server.arg("i_split").toFloat();
    if (server.hasArg("i_max")) pidConfig.INTEGRAL_MAX_LIMIT = server.arg("i_max").toFloat();
    if (server.hasArg("i_min")) pidConfig.INTEGRAL_MIN_LIMIT = server.arg("i_min").toFloat();

    // 超温保护参数
    if (server.hasArg("o_off")) pidConfig.OVERHEAT_PROTECT_OFF = server.arg("o_off").toFloat();

    // 触发自动保存
    pidConfig.needSave = true;

    // 提交成功后返回主页
    server.sendHeader("Location", "/", true);
    server.send(303, "text/plain", "Updated OK");
  });

  // ============================================================
  // 3. 实时温度 / PWM 数据接口
  // GET /data
  // ============================================================

  server.on("/data", HTTP_GET, []() {
    // 获取当前实际温度
    float temperature = current.currentTemp;

    // 获取 PWM 输出比例并转换成百分比
    float pwmPercent = current.pwmRate * 100.0f;

    // 检查数据是否有效
    if (!isfinite(temperature) || !isfinite(pwmPercent)) {
      server.send(503, "application/json", "{\"error\":\"invalid_data\"}");
      return;
    }

    // PWM 百分比限制在 0～100
    pwmPercent = constrain(pwmPercent, 0.0f, 100.0f);

    // 组装 JSON 数据
    String json = "{";
    json += "\"temperature\":" + String(temperature, 2) + ",";
    json += "\"pwm\":" + String(pwmPercent, 2);
    json += "}";

    // 禁止浏览器缓存实时数据
    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");

    // 返回 JSON
    server.send(200, "application/json", json);
  });

  // 启动 WebServer
  server.begin();

  print("HTTP Web Server started on port 8080");
}


// ============================================================
// 独立网页任务函数
// ============================================================

void webServerTaskCode(void* pvParameters) {

  // 等待 WiFi 连接成功
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  // 注册网页路由并启动服务器
  setupWebRoutes();

  while (1) {

    // 处理 HTTP 请求
    server.handleClient();

    // 释放 CPU 时间片
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}


// ============================================================
// 初始化并启动网页任务
// ============================================================

void initWebServerTask() {

  // 创建独立网页任务
  BaseType_t result = xTaskCreate(
    webServerTaskCode,
    "WebTask",
    4096,
    NULL,
    1,
    &WebServerTaskHandle);

  if (result != pdPASS) {

    print("ERROR: WebServer task creation failed!");
  }
}
