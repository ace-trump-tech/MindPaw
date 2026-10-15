//----------------------------------------------
//MindPaw Robot
//CodeVersion:V2.0  (MindPaw 2.0 — 流式 3D 重建感知层)
//---------------导入库--------------------------
#include <Arduino.h>
#include "board_config.h"
#include "servo_output.h"
#include <U8g2lib.h>
#include <Wire.h>
#include <FS.h>
#include <ESPAsyncWebServer.h>
#include "platform_network.h"
#include <NTPClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include "image.cpp"
//---------------新模块导入--------------------------
#include "hlkv20.h"             // HLK-V20 语音识别 (UART, 替代 LD3320 SPI)
#include "ov2640.h"
#include "speaker.h"
#include "doubao_agent.h"
#include "motion_emotion.h"
#include "emotion_engine.h"
#include "gesture_nn.h"
#include "multimodal_fusion.h"
#include "streaming_recon.h"    // 2.0: 流式 3D 重建感知层客户端
//---------------按键部分--------------------------
#define BUTTON_PIN MP_BUTTON
volatile bool buttonPressed = false;     // 按键标志
unsigned long lastPressTime = 0;         // 上次按键时间
const unsigned long debounceDelay = 50;  // 消抖时间 (ms)
//---------------ADC部分--------------------------
const float voltageDividerRatio = 8.4; // 分压比（8.4倍缩小）
const float minVoltage = 6.4; // 电压为0%时
const float maxVoltage = 8.4; // 电压为100%时
const int numSamples = 10;//定义采用次数
float batteryVoltage = 0; // 计算电池电压
int batteryPercentage = 0;//电量百分比
//---------------舵机部分--------------------------
MindPawServo servo1;//声明舵机1
MindPawServo servo2;//声明舵机2
MindPawServo servo3;//声明舵机3
MindPawServo servo4;//声明舵机4
int engine1 = 0; // PCA9685 channels, not GPIOs
int engine2 = 1;
int engine3 = 2;
int engine4 = 3;
//---------------屏幕部分--------------------------
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* clock=*/MP_SCL, /* data=*/MP_SDA); // SSD1306 uses the selected board I2C pins
//---------------网络部分--------------------------
const char *ssid = "MindPaw";//WIFI名称
const char *password = "mindpaw1234"; // AP 密码 (建议连接后修改)
AsyncWebServer server(80);//设置服务器端口
WiFiUDP ntpUDP;//声明UDP
NTPClient timeClient(ntpUDP, "ntp1.aliyun.com", 8 * 3600, 60000);//配置NTP服务器
//---------------API部分--------------------------
const char *weatherAPI = "http://api.seniverse.com/v3/weather/daily.json?key=";//心知天气API地址
String temperature = "";//天气温度
String humidity = "";//天气湿度
String weather = "";//天气
String cityname = "Shanghai";//城市名称
String weatherapi = ""; // 请填写心知天气API密钥
//---------------标签部分--------------------------
bool initweather = false;  // 天气初始化
bool freestate = false;//自由模式标签
int prevEmojiState = -1; // 用于跟踪之前的 emojiState
int actionstate = 0;//活动状态标签
int emojiState = 0; // 表情状态标签
//---------------新模块实例--------------------------
// 注意: 引脚定义在对应 .h 文件中，请根据实际接线调整
// 若某个模块未连接，注释掉其 begin() 调用即可
OV2640_Camera cameraModule;  // 摄像头模块 (OV2640)
Speaker speaker;             // 扬声器模块
HLKV20 voiceModule;          // HLK-V20 语音识别模块 (UART, 替代 LD3320)

// 语音命令 → 动作/表情映射状态
volatile int voiceActionState = 0;   // 语音触发的动作
volatile int voiceEmojiState = -1;   // 语音触发的表情 (-1=无变化)
volatile bool voiceTriggered = false;// 语音触发标志
volatile bool gestureTriggered = false; // 手势触发标志

// 扬声器互动标志
bool speakerReady = false;     // 扬声器是否可用
unsigned long lastInteractMs = 0; // 上次互动时间

//---------------AI Agent 部分--------------------------
DoubaoAgent aiAgent;            // 豆包 AI Agent
MotionEmotion motionEmotion;    // 情感动作模块

//---------------2.0: 流式 3D 重建感知层客户端--------------------------
// 默认 disable; 在 /aiConfig 中填入 Recon Gateway URL 后由 configureReconClient() 启用。
StreamingReconClient reconClient;
AgentState agentState = AGENT_IDLE; // Agent 状态
String pendingAgentText = "";   // 待处理文本
String pendingAgentRequestId = "";
String activeAgentRequestId = "";
String lastAgentRequestId = "";
String agentResultStatus = "idle"; // idle/queued/processing/completed/failed
bool pendingAgentInput = false; // 待处理标志
unsigned long agentCooldownMs = 0; // 请求冷却
String lastAgentReply = "";     // 最近 AI 回复
uint32_t agentRequestSeq = 0;

//---------------情感计算模块--------------------------
EmotionEngine emotionEngine;       // PAD 情感状态机
GestureNN gestureNN;               // 轻量手势识别引擎
MultimodalFusion fusion;           // 多模态融合层
uint8_t gestureFrameBuffer[1200];  // 40×30 灰度缓冲 (GestureNN 输入)

//---------------函数前向声明--------------------------
void processAgentInput(const String& text, bool contextReady = false);
void dispatchAgentResponse(const AgentResponse& resp);
void handleGestureNN(const GestureNNResult& nnResult);
//---------------文件系统部分--------------------------
const char *ssidFile = "/ssid.json";//配置存储文件名及路径
//---------------按键中断部分--------------------------
void MP_ISR_ATTR handleButtonPress()
{

    unsigned long currentTime = millis();// 获取当前系统运行时间（单位：毫秒）

    if (currentTime - lastPressTime > debounceDelay) // 检查按钮1是否满足去抖条件（避免机械抖动导致的误触发）
    {
        buttonPressed = true;     // 设置按钮1按下标志位
        lastPressTime = currentTime; // 更新按钮1的最后有效按下时间
    }
}
//---------------配置页面路由--------------------------
void handleWiFiConfig()
{

server.on("/front", HTTP_GET, [](AsyncWebServerRequest *request) {// 当访问 /front 路径时触发舵机动作
    actionstate = 1;  // 更新全局动作状态标志（1通常表示前进/前方动作）


    request->send(200, "text/plain", "Front function started"); // 立即响应客户端，避免阻塞（状态码200，返回纯文本确认信息）
});
//以下函数相同，不再注释
    server.on("/back", HTTP_GET, [](AsyncWebServerRequest *request)
              {
       actionstate = 4;   // 设置标志，执行舵机动作
        request->send(200, "text/plain", "Front function started"); });
    server.on("/left", HTTP_GET, [](AsyncWebServerRequest *request)
              {
       actionstate = 2;  // 设置标志，执行舵机动作
        request->send(200, "text/plain", "Front function started"); });
    server.on("/right", HTTP_GET, [](AsyncWebServerRequest *request)
              {
       actionstate = 3;  // 设置标志，执行舵机动作
        request->send(200, "text/plain", "Front function started"); });
    server.on("/toplefthand", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        actionstate = 5;   // 设置标志，执行舵机动作
        request->send(200, "text/plain", "Front function started"); });
    server.on("/toprighthand", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        actionstate = 6;  // 设置标志，执行舵机动作
        request->send(200, "text/plain", "Front function started"); });
    server.on("/sitdown", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        actionstate = 8;  // 设置标志，执行舵机动作
        request->send(200, "text/plain", "Front function started"); });
    server.on("/lie", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      actionstate = 7;
        request->send(200, "text/plain", "Front function started"); });
         server.on("/sleep", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      actionstate = 10;
        request->send(200, "text/plain", "Front function started"); });
    server.on("/free", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      freestate=true;
        request->send(200, "text/plain", "Front function started"); });
    server.on("/offfree", HTTP_GET, [](AsyncWebServerRequest *request)
              {
      freestate=false;
        request->send(200, "text/plain", "Front function started"); });
    server.on("/histate", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        emojiState = 0;  // 设置标志，执行舵机动作
        request->send(200, "text/plain", "Front function started"); });
    server.on("/angrystate", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        emojiState = 1;   // 设置标志，执行舵机动作
        request->send(200, "text/plain", "Front function started"); });
    server.on("/edastate", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        emojiState = 9;   // 设置标志，执行舵机动作
        request->send(200, "text/plain", "Front function started"); });

    server.on("/errorstate", HTTP_GET, [](AsyncWebServerRequest *request)
              {
       emojiState = 2;  // 设置标志，执行舵机动作
        request->send(200, "text/plain", "Front function started"); });
    server.on("/batteryVoltage", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", String(batteryVoltage)); });
    server.on("/batteryPercentage", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", String(batteryPercentage)); });
    //---------------新模块 Web 路由--------------------------
    server.on("/voiceStatus", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", voiceModule.isAvailable() ? "1" : "0"); });
    server.on("/cameraStatus", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", cameraModule.isAvailable() ? "1" : "0"); });
    server.on("/speakerStatus", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/plain", speaker.isPlaying() ? "1" : "0"); });
    server.on("/speakerMute", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        speaker.setMute(!speaker.isMuted());
        request->send(200, "text/plain", speaker.isMuted() ? "1" : "0"); });
    server.on("/lastInteraction", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String info = "Voice:" + String(voiceTriggered ? 1 : 0)
                    + ",Gesture:" + String(gestureTriggered ? 1 : 0)
                    + ",Since:" + String((millis() - lastInteractMs) / 1000);
        voiceTriggered = false;
        gestureTriggered = false;
        request->send(200, "text/plain", info); });
    //---------------AI Agent Web 路由--------------------------
    // AI 聊天入口: POST /aitalk (body: text=用户消息)
    server.on("/aitalk", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        if (!request->hasParam("text", true)) {
            request->send(400, "application/json", "{\"error\":\"missing text\"}");
            return;
        }
        String text = request->getParam("text", true)->value();
        text.trim();

        if (!aiAgent.isConfigured()) {
            String resp = "{\"reply\":\"AI未配置，请在设置页面配置API密钥和推理端点\"}";
            request->send(200, "application/json", resp);
            return;
        }

        if (text.length() == 0) {
            request->send(400, "application/json", "{\"error\":\"empty text\"}");
            return;
        }

        // 如果正在忙，提示等待
        if (pendingAgentInput || agentState == AGENT_BUSY) {
            request->send(200, "application/json", "{\"reply\":\"正在处理上一条消息，请稍候...\"}");
            return;
        }

        // processAgentInput owns text fusion so the emotion state is updated once.
        processAgentInput(text);

        DynamicJsonDocument queuedDoc(192);
        queuedDoc["status"] = "queued";
        queuedDoc["request_id"] = pendingAgentRequestId;
        String queuedJson;
        serializeJson(queuedDoc, queuedJson);
        request->send(202, "application/json", queuedJson); });

    // 轮询获取最新 AI 回复
    server.on("/aiReply", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        DynamicJsonDocument replyDoc(512);
        replyDoc["status"] = agentResultStatus;
        replyDoc["request_id"] = lastAgentRequestId;
        replyDoc["reply"] = lastAgentReply;
        String json;
        serializeJson(replyDoc, json);
        request->send(200, "application/json", json); });

    // AI 状态查询
    server.on("/aiStatus", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        String json = "{";
        json += "\"enabled\":" + String(aiAgent.isConfigured() ? "true" : "false") + ",";
        json += "\"busy\":" + String((agentState == AGENT_BUSY || pendingAgentInput) ? "true" : "false") + ",";
        json += "\"status\":\"" + agentResultStatus + "\",";
        json += "\"request_id\":\"" + lastAgentRequestId + "\",";
        json += "\"baseUrl\":\"" + aiAgent.getBaseUrl() + "\"";
        json += "}";
        request->send(200, "application/json", json); });

    // 情感状态查询 (PAD + 离散情感)
    server.on("/emotionStatus", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        const PADState& pad = emotionEngine.getState();
        String json = "{";
        json += "\"label\":\"" + String(emotionEngine.getEmotionLabel()) + "\",";
        json += "\"pleasure\":" + String(pad.pleasure, 2) + ",";
        json += "\"arousal\":" + String(pad.arousal, 2) + ",";
        json += "\"dominance\":" + String(pad.dominance, 2);
        json += "}";
        request->send(200, "application/json", json); });

    // AI 配置保存
    server.on("/aiConfig", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        String apiKey = request->hasParam("apiKey", true) ?
            request->getParam("apiKey", true)->value() : "";
        String endpointId = request->hasParam("endpointId", true) ?
            request->getParam("endpointId", true)->value() : "";
        String baseUrl = request->hasParam("baseUrl", true) ?
            request->getParam("baseUrl", true)->value() : "";
        apiKey.trim();
        endpointId.trim();
        baseUrl.trim();

        if (apiKey.length() == 0 || endpointId.length() == 0) {
            request->send(400, "application/json", "{\"status\":\"error\",\"error\":\"apiKey and endpointId are required\"}");
            return;
        }

        // 保存到 SPIFFS
        DynamicJsonDocument doc(384);
        doc["aiKey"] = apiKey;
        doc["aiEndpoint"] = endpointId;
        doc["aiBaseUrl"] = baseUrl;
        fs::File file = SPIFFS.open("/ai_config.json", "w");
        if (!file) {
            request->send(500, "application/json", "{\"status\":\"error\",\"error\":\"cannot open config storage\"}");
            return;
        }
        size_t written = serializeJson(doc, file);
        file.close();
        if (written == 0) {
            request->send(500, "application/json", "{\"status\":\"error\",\"error\":\"cannot write config storage\"}");
            return;
        }
        Serial.println("AI: Config saved");

        // 应用配置
        aiAgent.configure(apiKey, endpointId, baseUrl);

        request->send(200, "application/json", "{\"status\":\"ok\"}"); });

    // Clear only the in-memory conversation; credentials remain unchanged.
    server.on("/aiClearHistory", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        aiAgent.clearHistory();
        lastAgentReply = "";
        lastAgentRequestId = "";
        agentResultStatus = "idle";
        request->send(200, "application/json", "{\"status\":\"ok\"}"); });

    // AI 聊天页面
    server.on("/aichat.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (SPIFFS.exists("/aichat.html")) {
            fs::File file = SPIFFS.open("/aichat.html", "r");
            if (file) {
                String content;
                while (file.available()) content += (char)file.read();
                file.close();
                request->send(200, "text/html", content);
                return;
            }
        }
        request->send(404, "text/plain", "File Not Found"); });

    // AI 配置页面
    server.on("/aiconfig.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (SPIFFS.exists("/aiconfig.html")) {
            fs::File file = SPIFFS.open("/aiconfig.html", "r");
            if (file) {
                String content;
                while (file.available()) content += (char)file.read();
                file.close();
                request->send(200, "text/html", content);
                return;
            }
        }
        request->send(404, "text/plain", "File Not Found"); });
    server.on("/dowhatstate", HTTP_GET, [](AsyncWebServerRequest *request)
              {
       emojiState = 3;  // 设置标志，执行舵机动作
        request->send(200, "text/plain", "Front function started"); });
    server.on("/lovestate", HTTP_GET, [](AsyncWebServerRequest *request)
              {
       emojiState = 4;  // 设置标志，执行舵机动作
        request->send(200, "text/plain", "Front function started"); });
    server.on("/sickstate", HTTP_GET, [](AsyncWebServerRequest *request)
              {
       emojiState = 5;  // 设置标志，执行舵机动作
        request->send(200, "text/plain", "Front function started"); });
    server.on("/yunstate", HTTP_GET, [](AsyncWebServerRequest *request)
              {
       emojiState = 6;
        request->send(200, "text/plain", "Front function started"); });
    server.on("/time", HTTP_GET, [](AsyncWebServerRequest *request)
              {
       emojiState = 8;
        request->send(200, "text/plain", "Front function started"); });
    server.on("/weather", HTTP_GET, [](AsyncWebServerRequest *request)
              {
       emojiState = 7;  // 设置标志，执行舵机动作
        request->send(200, "text/plain", "Front function started"); });

    server.on("/connect", HTTP_POST, [](AsyncWebServerRequest *request)
              {
        // 获取POST参数：ssid、pass、city、api
        String ssid = request->getParam("ssid", true)->value();
        String pass = request->getParam("pass", true)->value();
        String city = request->getParam("city", true)->value();
        String api = request->getParam("api", true)->value();

        // 打印接收到的参数
        Serial.println(ssid);
        Serial.println(pass);

        // 保存WiFi信息到JSON文件
        DynamicJsonDocument doc(1024);
        doc["ssid"] = ssid;
        doc["pass"] = pass;
        doc["city"] = city;
        doc["api"] = api;
        fs::File file = SPIFFS.open(ssidFile, "w");  // 打开文件进行写入
        if (file) {
            serializeJson(doc, file);  // 将JSON内容写入文件
            file.close();  // 关闭文件
        }

        // 更新全局变量
        cityname = city;
        weatherapi = api;

        // 开始连接WiFi
        WiFi.begin(ssid.c_str(), pass.c_str());
        // 发送HTML响应，告知用户正在连接
        // 发送带UTF-8编码声明的HTML响应
request->send(200, "text/html; charset=UTF-8",
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "    <meta charset='UTF-8'>"
    "    <title>状态</title>"
    "</head>"
    "<body>"
    "    <h1>请返回使用在线功能，如果能正常获取则配置成功！</h1>"
    "</body>"
    "</html>"
);}
   );

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // 检查SPIFFS文件系统中是否存在index.html文件
        if (SPIFFS.exists("/index.html")) {
            fs::File file = SPIFFS.open("/index.html", "r");  // 打开index.html文件
            if (file) {
                size_t fileSize = file.size();  // 获取文件大小
                String fileContent;

                // 逐字节读取文件内容
                while (file.available()) {
                    fileContent += (char)file.read();
                }
                file.close();  // 关闭文件

                // 返回HTML内容
                request->send(200, "text/html", fileContent);
                return;
            }
        }
        // 如果文件不存在，返回404错误
        request->send(404, "text/plain", "File Not Found"); });
    server.on("/control.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // 检查SPIFFS文件系统中是否存在index.html文件
        if (SPIFFS.exists("/control.html")) {
            fs::File file = SPIFFS.open("/control.html", "r");  // 打开index.html文件
            if (file) {
                size_t fileSize = file.size();  // 获取文件大小
                String fileContent;

                // 逐字节读取文件内容
                while (file.available()) {
                    fileContent += (char)file.read();
                }
                file.close();  // 关闭文件

                // 返回HTML内容
                request->send(200, "text/html", fileContent);
                return;
            }
        }
        // 如果文件不存在，返回404错误
        request->send(404, "text/plain", "File Not Found"); });
    server.on("/motion.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (SPIFFS.exists("/motion.html")) {
            fs::File file = SPIFFS.open("/motion.html", "r");
            if (file) {
                size_t fileSize = file.size();
                String fileContent;
                while (file.available()) {
                    fileContent += (char)file.read();
                }
                file.close();
                request->send(200, "text/html", fileContent);
                return;
            }
        }
        request->send(404, "text/plain", "File Not Found"); });
    server.on("/expression.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (SPIFFS.exists("/expression.html")) {
            fs::File file = SPIFFS.open("/expression.html", "r");
            if (file) {
                size_t fileSize = file.size();
                String fileContent;
                while (file.available()) {
                    fileContent += (char)file.read();
                }
                file.close();
                request->send(200, "text/html", fileContent);
                return;
            }
        }
        request->send(404, "text/plain", "File Not Found"); });
    server.on("/network.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (SPIFFS.exists("/network.html")) {
            fs::File file = SPIFFS.open("/network.html", "r");
            if (file) {
                size_t fileSize = file.size();
                String fileContent;
                while (file.available()) {
                    fileContent += (char)file.read();
                }
                file.close();
                request->send(200, "text/html", fileContent);
                return;
            }
        }
        request->send(404, "text/plain", "File Not Found"); });
    server.on("/home.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (SPIFFS.exists("/home.html")) {
            fs::File file = SPIFFS.open("/home.html", "r");
            if (file) {
                size_t fileSize = file.size();
                String fileContent;
                while (file.available()) {
                    fileContent += (char)file.read();
                }
                file.close();
                request->send(200, "text/html", fileContent);
                return;
            }
        }
        request->send(404, "text/plain", "File Not Found"); });
    server.on("/egg.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (SPIFFS.exists("/egg.html")) {
            fs::File file = SPIFFS.open("/egg.html", "r");
            if (file) {
                size_t fileSize = file.size();
                String fileContent;
                while (file.available()) {
                    fileContent += (char)file.read();
                }
                file.close();
                request->send(200, "text/html", fileContent);
                return;
            }
        }
        request->send(404, "text/plain", "File Not Found"); });
    server.on("/engine.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // 检查SPIFFS文件系统中是否存在index.html文件
        if (SPIFFS.exists("/engine.html")) {
            fs::File file = SPIFFS.open("/engine.html", "r");  // 打开index.html文件
            if (file) {
                size_t fileSize = file.size();  // 获取文件大小
                String fileContent;

                // 逐字节读取文件内容
                while (file.available()) {
                    fileContent += (char)file.read();
                }
                file.close();  // 关闭文件

                // 返回HTML内容
                request->send(200, "text/html", fileContent);//发送文件内容
                return;
            }
        }
        // 如果文件不存在，返回404错误
        request->send(404, "text/plain", "File Not Found"); });
    server.on("/setting.html", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        // 检查SPIFFS文件系统中是否存在index.html文件
        if (SPIFFS.exists("/setting.html")) {
            fs::File file = SPIFFS.open("/setting.html", "r");  // 打开index.html文件
            if (file) {
                size_t fileSize = file.size();  // 获取文件大小
                String fileContent;

                // 逐字节读取文件内容
                while (file.available()) {
                    fileContent += (char)file.read();
                }
                file.close();  // 关闭文件

                // 返回HTML内容
                request->send(200, "text/html", fileContent);
                return;
            }
        }
        // 如果文件不存在，返回404错误
        request->send(404, "text/plain", "File Not Found"); });

    // ============================================================
    // 2.0: 流式 3D 重建感知层 endpoints (不破坏 1.0 任何 handler)
    // ============================================================

    // 设备主动 poll: 拉取最新 hazard 决策 (给情感引擎 / OLED 显示用)
    server.on("/recon/hazard", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<192> doc;
        doc["hazard"]     = reconClient.getLastHazard();
        doc["nearest_m"]  = reconClient.getLastNearestM();
        doc["drift_cm"]   = reconClient.getLastDriftCm();
        doc["frame_id"]   = reconClient.getLastFrameId();
        doc["streaming"]  = reconClient.isStreaming();
        String body;
        serializeJson(doc, body);
        request->send(200, "application/json", body);
    });

    // 启动推流循环 (在 setup 完 / 用户配置 URL 后调用一次)
    server.on("/recon/start", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool ok = reconClient.startStreaming();
        request->send(ok ? 200 : 503, "application/json",
                      String("{\"status\":\"") + (ok ? "streaming" : "failed") + "\"}");
    });

    // 停止推流 (断电或 web 端按钮调用)
    server.on("/recon/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool ok = reconClient.stopStreaming();
        request->send(ok ? 200 : 503, "application/json",
                      String("{\"status\":\"") + (ok ? "stopped" : "failed") + "\"}");
    });

    // 浏览器查看器入口 (与 recon_view.html SPIFFS handler 平行; 两者都可访问)
    server.on("/recon_view.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (SPIFFS.exists("/recon_view.html")) {
            fs::File file = SPIFFS.open("/recon_view.html", "r");
            if (file) {
                String content;
                while (file.available()) content += (char)file.read();
                file.close();
                request->send(200, "text/html", content);
                return;
            }
        }
        request->send(404, "text/plain", "File Not Found");
    });

    // 启动服务器
    server.begin();
};
void loadWiFiConfig()
{
    // 初始化SPIFFS文件系统（存储WiFi配置等信息）
if (SPIFFS.begin()) // 成功挂载文件系统
{
    // 尝试打开存储WiFi配置的JSON文件（需提前创建）
    fs::File file = SPIFFS.open(ssidFile, "r"); // "r"表示只读模式
    if (file) // 文件存在且可访问
    {
        // 创建动态JSON文档（容量需根据实际配置数据调整）
        DynamicJsonDocument doc(1024); // 建议至少1024字节存储配置参数

        // 反序列化JSON数据（将文件内容解析为JSON对象）
        DeserializationError error = deserializeJson(doc, file);

        if (!error) // JSON解析成功
        {
            // 从JSON对象中提取配置参数
            String ssid = doc["ssid"];     // WiFi名称字段
            String pass = doc["pass"];     // WiFi密码字段
            String city = doc["city"];     // 城市代码字段
            String api = doc["api"];       // 天气API密钥字段

            // 将配置参数赋给全局变量
            cityname = city;        // 存储城市代码
            weatherapi = api;       // 存储API密钥

            // 使用存储的凭证尝试连接WiFi
            WiFi.begin(ssid.c_str(), pass.c_str()); // 转换为C风格字符串

            // 设置5秒连接超时（5000ms）
            unsigned long startAttemptTime = millis();
            while (WiFi.status() != WL_CONNECTED &&
                  millis() - startAttemptTime < 5000)
            {
                delay(500); // 等待连接，每0.5秒检测一次
            }

            // 连接状态检测
            if (WiFi.status() != WL_CONNECTED)
            {
                Serial.println("WiFi connection failed, starting captive portal...");
                handleWiFiConfig(); // 启动强制配置门户（如AP模式）
            }
            else
            {
                Serial.println("WiFi connected");
                timeClient.begin(); // 初始化NTP时间客户端
            }
        }
        file.close(); // 关闭文件释放资源
    }
}

}
void fetchWeather()
{ // 天气捕捉
    // 天气数据初始化模块（首次运行或需要更新时触发）
if (initweather == false)
{
    // 检测WiFi连接状态（确保网络可用性）
    if (WiFi.status() == WL_CONNECTED)
    {
        WiFiClient client;      // 创建TCP客户端
        HTTPClient http;        // 初始化HTTP客户端

        // 构建带参数的API请求URL（包含动态参数）
        String apiUrl = weatherAPI + weatherapi + "&location=" + cityname + "&language=zh-Hans&unit=c&start=0&days=1";

        // 发起HTTPS连接（注意：实际需确认weatherAPI是否支持SSL）
        if (http.begin(client, apiUrl))
        {
            int httpCode = http.GET();  // 发送GET请求

            // 成功接收响应（httpCode 200表示成功）
            if (httpCode > 0)
            {
                String payload = http.getString();  // 获取完整响应数据

                // 调试输出原始JSON数据（建议在开发阶段开启）
                Serial.println("JSON Response:");
                Serial.println(payload);

                // 创建JSON文档并解析数据
                DynamicJsonDocument doc(2048);  // 建议扩大至2048字节防止数据截断
                DeserializationError error = deserializeJson(doc, payload);

                if (!error)
                {
                    // 提取天气数据（注意字段路径需与API响应结构匹配）
                    String temperature2 = doc["results"][0]["daily"][0]["high"];     // 最高温度
                    String humidity2 = doc["results"][0]["daily"][0]["humidity"];     // 湿度值
                    String weathe2r = doc["results"][0]["daily"][0]["text_day"];     // 天气描述（变量名疑似拼写错误）

                    // 更新全局天气变量
                    temperature = temperature2;
                    humidity = humidity2;
                    weather = weathe2r;
                    initweather = true;  // 标记已完成初始化

                    // 调试输出解析结果
                    Serial.print("Data received: ");
                    Serial.println(temperature);
                    Serial.println(humidity);
                    Serial.println(weather);
                }
                else
                {
                    Serial.println("JSON解析失败: " + String(error.c_str()));
                }
            }
            else
            {
                Serial.printf("HTTP请求失败，错误代码: %d，详情: %s\n",
                            httpCode, http.errorToString(httpCode).c_str());
            }
            http.end();  // 必须释放资源
        }
        else
        {
            Serial.println("服务器连接失败，请检查API地址");
        }
    }
}
    if (weather == "小雨" || weather == "大雨" || weather == "暴雨" || weather == "雨")//识别天气
    {
        do
        {
            u8g2.setFont(u8g2_font_ncenB08_tr);//配置字体
            u8g2.drawXBMP(0, 0, 64, 64, rain);//展示图片
            u8g2.drawStr(64, 20, "Temp");//显示温度
            String temperatureString = String(temperature) + " C";//拼接字符串
            u8g2.drawStr(64, 30, temperatureString.c_str());//屏幕显示
            u8g2.drawStr(64, 50, "Humidity");//内容同上不再注释
            String humidityString = String(humidity) + " %";
            u8g2.drawStr(64, 60, humidityString.c_str());
        } while (u8g2.nextPage());
    }
    else if (weather == "晴")
    {
        do
        {
            u8g2.setFont(u8g2_font_ncenB08_tr);
            u8g2.drawStr(64, 20, "Temp");
            u8g2.drawXBMP(0, 0, 64, 64, sun);
            String temperatureString = String(temperature) + " %";
            u8g2.drawStr(64, 30, temperatureString.c_str());
            u8g2.drawStr(64, 50, "Humidity");
            String humidityString = String(humidity) + " %";
            u8g2.drawStr(64, 60, humidityString.c_str());
        } while (u8g2.nextPage());
    }
    else
    {
        do
        {
            u8g2.setFont(u8g2_font_ncenB08_tr);
            u8g2.drawXBMP(0, 0, 64, 64, cloud);
            u8g2.drawStr(64, 20, "Temp");
            String temperatureString = String(temperature) + " C";
            u8g2.drawStr(64, 30, temperatureString.c_str());
            u8g2.drawStr(64, 50, "Humidity");
            String humidityString = String(humidity) + " %";
            u8g2.drawStr(64, 60, humidityString.c_str());
        } while (u8g2.nextPage());
    }
}

void front()
{
    servo2.write(140); //舵机2旋转至140度
    servo3.write(40);  //舵机旋转至40度
    delay(100);//延时100s
    servo1.write(40);  //内容同上
    servo4.write(140);
    delay(100);
    servo2.write(90);
    servo3.write(90);
    delay(100);
    servo1.write(90);
    servo4.write(90);
    delay(100);
    servo1.write(140);
    servo4.write(40);
    delay(100);
    servo2.write(40);
    servo3.write(140);
    delay(100);
    servo1.write(90);
    servo4.write(90);
    delay(100);
    servo2.write(90);
    servo3.write(90);

}
void back()
{
    servo3.write(140);
    servo2.write(40);
    delay(100);
    servo4.write(40);
    servo1.write(140);
    delay(100);
    servo3.write(90);
    servo2.write(90);
    delay(100);
    servo4.write(90);
    servo1.write(90);
    delay(100);
    servo4.write(140);
    servo1.write(40);
    delay(100);
    servo3.write(40);
    servo2.write(140);
    delay(100);
    servo4.write(90);
    servo1.write(90);
    delay(100);
    servo3.write(90);
    servo2.write(90);

}
void right()
{
    int num = 0;
    while (num < 3)//调用一次执行3次
    {
        servo1.write(100);
        servo4.write(100);
        delay(100);
        servo3.write(60);
        servo2.write(60);
        delay(100);
        servo1.write(140);
        servo4.write(140);
        delay(100);
        servo3.write(40);
        servo2.write(40);
        delay(100);
        servo3.write(90);
        servo2.write(90);
        servo1.write(90);
        servo4.write(90);
        delay(100);
        servo1.write(80);
        servo4.write(80);
        delay(100);
        servo3.write(120);
        servo2.write(120);
        delay(100);
        servo1.write(90);
        servo4.write(90);
        delay(100);
        servo3.write(140);
        servo2.write(140);
        delay(100);
        servo3.write(90);
        servo2.write(90);

        num++;
    }

}
void left()
{

    int num = 0;
    while (num < 3)
    {
        servo1.write(80);
        servo4.write(80);
        delay(100);
        servo3.write(120);
        servo2.write(120);
        delay(100);
        servo1.write(40);
        servo4.write(40);
        delay(100);
        servo3.write(140);
        servo2.write(140);
        delay(100);
        servo3.write(90);
        servo2.write(90);
        servo1.write(90);
        servo4.write(90);
        delay(100);
        servo1.write(100);
        servo4.write(100);
        delay(100);
        servo3.write(60);
        servo2.write(60);
        delay(100);
        servo1.write(90);
        servo4.write(90);
        delay(100);
        servo3.write(40);
        servo2.write(40);
        delay(100);
        servo3.write(90);
        servo2.write(90);

        num++;
    }
}

void sitdown()
{
    servo2.write(140);
    servo4.write(40);
    delay(3000);
    servo2.write(90);
    servo4.write(90);

}
void lie()
{
    servo1.write(180);
    servo3.write(0);
    servo2.write(0);
    servo4.write(180);
    delay(3000);
    servo1.write(90);
    servo3.write(90);
    servo2.write(90);
    servo4.write(90);

}

void toplefthand()
{
    int num = 0;
    while (num < 3)
    {
        servo3.write(0);
        delay(100);
        servo3.write(30);
        delay(100);

        num++;
    }
    servo3.write(90);
}
void toprighthand()
{

    int num = 0;
    while (num < 3)
    {
        servo1.write(180);
        delay(100);
        servo1.write(150);
        delay(100);

        num++;
    }
    servo1.write(90);
}
void dosleep()
{
    servo1.write(0);
    servo3.write(180);
    servo2.write(180);
    servo4.write(0);
}

//---------------语音命令处理--------------------------
void handleVoiceCommand(VoiceCommand cmd) {
    // 扬声器反馈
    speaker.play(MELODY_COMMAND_OK);
    lastInteractMs = millis();

    Serial.print("Voice command: ");
    Serial.println(cmd);

    switch (cmd) {
        // ----- 运动命令 (走快速通道) -----
        case VOICE_FORWARD:
            actionstate = 1;
            speaker.play(MELODY_BEEP);
            break;
        case VOICE_BACKWARD:
            actionstate = 4;
            speaker.play(MELODY_BEEP);
            break;
        case VOICE_LEFT:
            actionstate = 2;
            speaker.play(MELODY_BEEP);
            break;
        case VOICE_RIGHT:
            actionstate = 3;
            speaker.play(MELODY_BEEP);
            break;
        case VOICE_STOP:
            actionstate = 0;  // 停止所有动作
            freestate = false;
            speaker.play(MELODY_BEEP);
            break;
        case VOICE_SIT:
            actionstate = 8;
            speaker.play(MELODY_BEEP);
            break;
        case VOICE_LIE:
            actionstate = 7;
            speaker.play(MELODY_BEEP);
            break;
        case VOICE_SLEEP:
            actionstate = 10;
            speaker.play(MELODY_BEEP);
            break;
        case VOICE_HAND_LEFT:
            actionstate = 5;
            speaker.play(MELODY_BEEP);
            break;
        case VOICE_HAND_RIGHT:
            actionstate = 6;
            speaker.play(MELODY_BEEP);
            break;
        case VOICE_FREE:
            freestate = !freestate;  // 切换自由模式
            speaker.play(freestate ? MELODY_HAPPY : MELODY_DONE);
            break;

        // ----- 社交/表情命令 (AI 已启用时走 Agent + 情感融合，否则直接执行) -----
        case VOICE_HELLO:
            if (aiAgent.isConfigured()) {
                { MultimodalContext ctx = fusion.processVoice(cmd);
                  aiAgent.setAffectiveContext(fusion.buildAffectivePrompt(ctx));
                  processAgentInput(ctx.userText, true); }
            } else {
                emojiState = 0;
                speaker.play(MELODY_HELLO);
            }
            break;
        case VOICE_HAPPY:
            if (aiAgent.isConfigured()) {
                { MultimodalContext ctx = fusion.processVoice(cmd);
                  aiAgent.setAffectiveContext(fusion.buildAffectivePrompt(ctx));
                  processAgentInput(ctx.userText, true); }
            } else {
                emojiState = 0;
                speaker.play(MELODY_HAPPY);
            }
            break;
        case VOICE_SAD:
            if (aiAgent.isConfigured()) {
                { MultimodalContext ctx = fusion.processVoice(cmd);
                  aiAgent.setAffectiveContext(fusion.buildAffectivePrompt(ctx));
                  processAgentInput(ctx.userText, true); }
            } else {
                emojiState = 5;
                speaker.play(MELODY_SAD);
            }
            break;
        case VOICE_ANGRY:
            if (aiAgent.isConfigured()) {
                { MultimodalContext ctx = fusion.processVoice(cmd);
                  aiAgent.setAffectiveContext(fusion.buildAffectivePrompt(ctx));
                  processAgentInput(ctx.userText, true); }
            } else {
                emojiState = 1;
            }
            break;
        case VOICE_CURIOUS:
            if (aiAgent.isConfigured()) {
                { MultimodalContext ctx = fusion.processVoice(cmd);
                  aiAgent.setAffectiveContext(fusion.buildAffectivePrompt(ctx));
                  processAgentInput(ctx.userText, true); }
            } else {
                emojiState = 3;
            }
            break;
        case VOICE_LOVE:
            if (aiAgent.isConfigured()) {
                { MultimodalContext ctx = fusion.processVoice(cmd);
                  aiAgent.setAffectiveContext(fusion.buildAffectivePrompt(ctx));
                  processAgentInput(ctx.userText, true); }
            } else {
                emojiState = 4;
                speaker.play(MELODY_HAPPY);
            }
            break;
        case VOICE_BYE:
            if (aiAgent.isConfigured()) {
                { MultimodalContext ctx = fusion.processVoice(cmd);
                  aiAgent.setAffectiveContext(fusion.buildAffectivePrompt(ctx));
                  processAgentInput(ctx.userText, true); }
            } else {
                emojiState = 0;
                speaker.play(MELODY_GOODBYE);
            }
            break;

        // ----- 信息查询 (AI 优先) -----
        case VOICE_TIME:
            if (aiAgent.isConfigured()) {
                { MultimodalContext ctx = fusion.processVoice(cmd);
                  aiAgent.setAffectiveContext(fusion.buildAffectivePrompt(ctx));
                  processAgentInput("现在几点了", true); }
            } else {
                emojiState = 8;
            }
            break;
        case VOICE_WEATHER:
            if (aiAgent.isConfigured()) {
                processAgentInput("今天天气怎么样");
            } else {
                emojiState = 7;
            }
            break;
        case VOICE_LOGO:
            emojiState = 9;
            break;

        default:
            speaker.play(MELODY_ERROR);
            break;
    }

    voiceTriggered = true;
}

//---------------手势识别处理 (轻量 MLP) --------------------------
void handleGestureNN(const GestureNNResult& nnResult) {
    if (nnResult.gesture == GESTURE_NONE) return;

    Serial.printf("GESTURE: Classified=%d (conf=%.2f)\n", nnResult.gesture, nnResult.confidence);

    speaker.play(MELODY_BEEP);
    lastInteractMs = millis();

    // 1. 通过多模态融合层更新情感
    MultimodalContext ctx = fusion.processGesture((int8_t)nnResult.gesture, nnResult.confidence);

    // 2. AI 已启用时，构建情感上下文发送给 Agent
    if (aiAgent.isConfigured()) {
        aiAgent.setAffectiveContext(fusion.buildAffectivePrompt(ctx));
        processAgentInput(ctx.userText, true);
        return;
    }

    // 3. AI 未启用时，手势直接映射为动作
    switch (nnResult.gesture) {
        case GESTURE_WAVE:
            freestate = !freestate;
            speaker.play(freestate ? MELODY_HAPPY : MELODY_DONE);
            break;
        case GESTURE_PALM:
            actionstate = 7;  // 趴下 (停)
            break;
        case GESTURE_FIST:
            actionstate = 12; // 点头 (同意)
            break;
        case GESTURE_POINT:
            actionstate = 14; // 鞠躬 (礼貌)
            break;
        default:
            break;
    }

    gestureTriggered = true;
}

//---------------统一 AI Agent 入口--------------------------
// 所有输入 (网页/语音/手势) 都通过此函数发送给 AI
void processAgentInput(const String& text, bool contextReady) {
    if (text.length() == 0) return;

    // 如果 AI 未配置或正在忙，忽略
    if (!aiAgent.isEnabled() || agentState != AGENT_IDLE) {
        Serial.println("AI: Agent busy or disabled, ignoring input");
        return;
    }

    // 通过多模态融合层处理 (构建情感上下文)
    MultimodalContext ctx;
    if (contextReady) {
        ctx = fusion.getLastContext();
        ctx.userText = text;
    } else {
        ctx = fusion.processText(text);
        aiAgent.setAffectiveContext(fusion.buildAffectivePrompt(ctx));
    }

    // 更新情感强度到运动模块
    const PADState& pad = emotionEngine.getState();
    motionEmotion.setEmotionalIntensity(pad.pleasure, pad.arousal);

    // 存储待处理文本
    pendingAgentText = ctx.userText;
    agentRequestSeq++;
    if (agentRequestSeq == 0) agentRequestSeq = 1;
    pendingAgentRequestId = String("mp-") + String(agentRequestSeq);
    lastAgentRequestId = pendingAgentRequestId;
    lastAgentReply = "";
    agentResultStatus = "queued";
    pendingAgentInput = true;

    Serial.printf("AI: Queued with affective context: \"%s\"\n", ctx.emotionDescription.c_str());
}

//---------------AI 响应分发--------------------------
void dispatchAgentResponse(const AgentResponse& resp) {
    Serial.printf("AI: Dispatch reply=\"%s\" action=%d expr=%d melody=%d repeat=%d\n",
        resp.reply_text.c_str(), resp.action, resp.expression,
        resp.melody, resp.repeat);

    // 1. 保存回复文本 (供 Web UI 轮询)
    lastAgentReply = resp.reply_text;

    // 2. 动作 (action >= 11 为情感动作，否则走 actionstate)
    if (resp.action >= 11 && resp.action <= 18) {
        // 情感动作 — 用 motionEmotion 非阻塞执行
        EmotionAction emotionAction = (EmotionAction)resp.action;
        motionEmotion.trigger(emotionAction, max((uint8_t)1, resp.repeat));
    } else if (resp.action >= 1 && resp.action <= 10) {
        // 标准动作 — 走现有 actionstate
        actionstate = resp.action;
    }

    // 3. 表情
    if (resp.expression >= 0 && resp.expression <= 9) {
        emojiState = resp.expression;
    }

    // 4. 旋律
    if (resp.melody >= 0 && resp.melody <= 9) {
        speaker.play((Melody)resp.melody);
    }

    // 5. 情感反馈 → 更新 PAD 情感状态机
    if (resp.emotion >= 1 && resp.emotion <= 5) {
        emotionEngine.updateFromLLMResponse(resp.emotion);
        // 同步情感强度到运动模块
        const PADState& pad = emotionEngine.getState();
        motionEmotion.setEmotionalIntensity(pad.pleasure, pad.arousal);
        Serial.printf("EMOTION: Updated from LLM response: %s\n", emotionEngine.getEmotionLabel());
    }
}

// 对 ADC 数据多次采样并计算平均值
float getAverageAdcVoltage()
{
    long totalAdcValue = 0;

    // 多次采样
    for (int i = 0; i < numSamples; i++)
    {
#if defined(ESP32)
        totalAdcValue += analogReadMilliVolts(MP_BATTERY_ADC);
#else
        totalAdcValue += analogRead(MP_BATTERY_ADC);
#endif
        delay(10);                       // 每次采样间隔 10ms
    }

    // 计算平均 ADC 值
    float averageAdcValue = totalAdcValue / (float)numSamples;

    // 将 ADC 值转换为电压
#if defined(ESP32)
    return averageAdcValue / 1000.0f;
#else
    return (averageAdcValue / 1023.0) * 1.0;
#endif
}

// 计算电池电量百分比的函数
int mapBatteryPercentage(float voltage)
{
    if (voltage <= minVoltage)
        return 0; // 小于等于最小电压时，电量为 0%
    if (voltage >= maxVoltage)
        return 100; // 大于等于最大电压时，电量为 100%

    // 根据线性比例计算电量百分比
    return (int)((voltage - minVoltage) / (maxVoltage - minVoltage) * 100);
}
void serialListen(){

    // 读取完整字符串（直到换行符）
    String receivedString = Serial.readStringUntil('\n');
    // 去掉可能的回车符或空格
    receivedString.trim();
    // 处理接收到的字符串
    Serial.print("Received: ");
    Serial.println(receivedString);

    // Diagnostics run in loop, never concurrently in an async web callback.
    if (receivedString == "camera jpeg" || receivedString == "camera gray") {
        if (!cameraModule.isAvailable()) {
            Serial.println("CAMERA_ERROR unavailable");
            return;
        }
        if (receivedString == "camera gray") {
            if (!cameraModule.captureGrayscale(gestureFrameBuffer)) {
                Serial.println("CAMERA_ERROR capture");
                return;
            }
            Serial.println("CAMERA_GRAY 1200");
            Serial.write(gestureFrameBuffer, sizeof(gestureFrameBuffer));
        } else {
            uint8_t* frame = static_cast<uint8_t*>(malloc(RECON_MAX_JPEG_BYTES));
            size_t length = 0;
            if (!frame || !cameraModule.captureJpeg(frame, RECON_MAX_JPEG_BYTES, length)) {
                free(frame);
                Serial.println("CAMERA_ERROR capture_or_capacity");
                return;
            }
            Serial.printf("CAMERA_JPEG %u\n", unsigned(length));
            Serial.write(frame, length);
            free(frame);
        }
        Serial.println();
        return;
    }

    // 支持 "ask <文本>" 命令 — 发送给 AI Agent
    if (receivedString.startsWith("ask ")) {
        String question = receivedString.substring(4);
        question.trim();
        if (question.length() > 0) {
            Serial.printf("AI: Question via serial: \"%s\"\n", question.c_str());
            processAgentInput(question);
        }
        return;
    }
    // 支持 "ai clear" 命令 — 清除对话历史
    if (receivedString == "ai clear") {
        aiAgent.clearHistory();
        Serial.println("AI: History cleared");
        return;
    }
    // 支持 "emotion" 命令 — 显示当前情感状态
    if (receivedString == "emotion") {
        const PADState& pad = emotionEngine.getState();
        Serial.printf("EMOTION: %s (P=%.2f A=%.2f D=%.2f)\n",
                     emotionEngine.getEmotionLabel(),
                     pad.pleasure, pad.arousal, pad.dominance);
        return;
    }
    // 支持 "emotion reset" 命令 — 重置情感状态
    if (receivedString == "emotion reset") {
        emotionEngine.reset();
        Serial.println("EMOTION: Reset to baseline");
        return;
    }
    // 支持 "gesture" 命令 — 触发一次手势识别
    if (receivedString == "gesture") {
        if (cameraModule.isAvailable()) {
            if (cameraModule.captureGrayscale(gestureFrameBuffer)) {
                GestureNNResult nnResult = gestureNN.classify(gestureFrameBuffer);
                Serial.printf("GESTURE_NN: Class=%d conf=%.2f\n",
                             nnResult.gesture, nnResult.confidence);
            }
        } else {
            Serial.println("GESTURE: Camera not available");
        }
        return;
    }

    if(receivedString=="front"){//当接收到"front"时
        front();//执行前进
    };
    if(receivedString=="back"){
        back();
    };
    if(receivedString=="toplefthand"){
        toplefthand();
    };
    if(receivedString=="toprighthand"){
        toprighthand();
    };
    if(receivedString=="left"){
        left();
    };
    if(receivedString=="right"){
        right();
    };
    if(receivedString=="sitdown"){
        sitdown();
    };
    if(receivedString=="lie"){
        lie();
    };
    if(receivedString=="dosleep"){
        dosleep();
    };
    if(receivedString=="kaixin"){
        emojiState=0;
    };
    if(receivedString=="shengqi"){
        emojiState=1;
    };
    if(receivedString=="nanshou"){
        emojiState=5;
    };
    if(receivedString=="haoqi"){
        emojiState=3;
    };
    if(receivedString=="xihuan"){
        emojiState=4;
    };
    if(receivedString=="cuowu"){
        emojiState=2;
    };
    if(receivedString=="yun"){
        emojiState=6;
    };
    if(receivedString=="shijian"){
        emojiState=8;
    };
    if(receivedString=="tianqi"){
        emojiState=7;
    };
    if(receivedString=="logo"){
        emojiState=9;
    };


}
void setup()
{
Serial.begin(115200);
Wire.begin(MP_SDA, MP_SCL);
u8g2.begin();
u8g2.setDisplayRotation(U8G2_R2);
// OLED 显示初始化与按钮中断配置
u8g2.firstPage();  // 启动U8g2页面缓冲绘制
do {
    // 设置显示字体（14像素高度，适合128x64屏幕）
    u8g2.setFont(u8g2_font_ncenB14_tr);

    // 绘制LOGO位图（居中显示计算）
    // 参数说明：X坐标0（左对齐），Y坐标(64/2 -22/2)=21（垂直居中）
    // 位图尺寸：宽度128px，高度22px，数据源为logo数组
    u8g2.drawXBMP(0, (64 / 2 - 22 / 2), 128, 22, logo);
} while (u8g2.nextPage());  // 循环刷新直至完成全帧绘制

// 按钮1配置（通常对应GPIO2）
// 硬件要求：按钮接地触发，内置上拉保持高电平
pinMode(BUTTON_PIN, INPUT_PULLUP);
// 配置下降沿中断（物理按下时产生低电平）
attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, FALLING);

// GPIO15 is camera CS; no second button on this pin.
//启用SPIFFS文件系统
    SPIFFS.begin();
    servo1.attach(engine1, 500, 2500); // 配置舵机PWM，500µs=0度，2500µs=180度
    servo2.attach(engine2, 500, 2500);
    servo3.attach(engine3, 500, 2500);
    servo4.attach(engine4, 500, 2500);
    servo1.write(90);//舵机旋转到90度
    servo3.write(90);
    servo2.write(90);
    servo4.write(90);

    //---------------初始化新模块--------------------------
    // 初始化扬声器 (开机音效)
    speaker.begin();
    speaker.play(MELODY_STARTUP);
    speakerReady = true;

    // 初始化语音识别模块 (HLK-V20, UART)
    // 替代 LD3320 (SPI), 仅需 1 个引脚
    // TX→D3 (GPIO0), RX→可选(仅配置时用)
    if (voiceModule.begin(HLKV20_RX_PIN, 9600)) {
        Serial.println("HLK-V20: Voice module ready (UART)");
        voiceModule.setDebug(false);
    } else {
        Serial.println("HLK-V20: Voice module not found (skip)");
    }

    // 初始化摄像头模块 (OV2640 + 灰度捕获)
    // 用于 GestureNN 轻量手势识别
    if (cameraModule.begin()) {
        Serial.println("OV2640: Camera ready");
        cameraModule.setDebug(false);
        cameraModule.setInterval(800);     // 每 800ms 捕获一帧
    } else {
        Serial.println("OV2640: Camera not found (skip)");
    }

    //---------------初始化情感计算引擎--------------------------
    emotionEngine.setDebug(false);
    emotionEngine.setPersonality(
        0.75f,   // extraversion — 外倾，喜欢互动
        0.80f,   // agreeableness — 宜人，友好
        0.60f,   // conscientiousness — 尽责
        0.30f,   // neuroticism — 低神经质，情绪稳定
        0.70f    // openness — 开放，好奇
    );
    emotionEngine.setDecayRate(0.97f, 0.98f, 0.99f);  // 缓慢回归中性
    Serial.println("EMOTION: PAD engine initialized");

    // 连接多模态融合层 → 情感引擎
    fusion.attachEmotionEngine(&emotionEngine);
    fusion.setDebug(false);

    // 手势识别引擎 (权重编译在代码中)
    gestureNN.setDebug(false);
    Serial.println("GESTURE_NN: Lightweight MLP ready");

    //---------------初始化 AI Agent + 情感动作--------------------------
    // 关联情感动作到舵机
    motionEmotion.attach(servo1, servo2, servo3, servo4);

    // 从 SPIFFS 加载 AI 配置 (aiKey, aiEndpoint)
    if (SPIFFS.exists("/ai_config.json")) {
        fs::File aiFile = SPIFFS.open("/ai_config.json", "r");
        if (aiFile) {
            DynamicJsonDocument aiDoc(384);
            DeserializationError aiErr = deserializeJson(aiDoc, aiFile);
            if (!aiErr) {
                String aiKey = aiDoc["aiKey"] | "";
                String aiEndpoint = aiDoc["aiEndpoint"] | "";
                String aiBaseUrl = aiDoc["aiBaseUrl"] | "";
                if (aiKey.length() > 0 && aiEndpoint.length() > 0) {
                    aiAgent.configure(aiKey, aiEndpoint, aiBaseUrl);
                    Serial.println("DOUBAO: AI agent configured from SPIFFS");
                }
            }
            aiFile.close();
        }
    } else {
        Serial.println("DOUBAO: No AI config file found");
    }

    // 设置WiFi为热点模式
    WiFi.softAP(ssid, password);
    Serial.println("热点已启动");
    // 访问的IP地址是 ESP8266 的默认IP：192.168.4.1
    Serial.print("访问地址: ");
    Serial.print(WiFi.softAPIP());
    // 加载WiFi配置
    loadWiFiConfig();
    if (WiFi.status() != WL_CONNECTED)//当WIFI未连接时
    {
        Serial.println("Starting captive portal...");//串口输出
        handleWiFiConfig();//加载WIFI配置
    }
    else
    {
        handleWiFiConfig();//加载WIFI配置
        Serial.println("WiFi connected");
        timeClient.begin();//NTP服务初始化
        timeClient.update(); // 获取初始时间
    }
    delay(5000);
    u8g2.clearDisplay();//清屏
    do
    {
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.drawXBMP(0, 0, 64, 64, ipip);
        u8g2.drawStr(64, 10, "WIFI_AP");
        u8g2.drawStr(64, 27, "MindPaw");
        u8g2.drawStr(64, 43, "192.168.4.1");
        u8g2.drawStr(64, 60, "WIFI CTRL");
    } while (u8g2.nextPage());
    delay(5000);
    u8g2.clearDisplay();
}

void loop()
{
    if (Serial.available() > 0) {
        serialListen();
    }

    //---------------语音识别检测--------------------------
    if (voiceModule.isAvailable()) {
        VoiceCommand voiceCmd = voiceModule.loop();
        if (voiceCmd != VOICE_NONE) {
            handleVoiceCommand(voiceCmd);
        }
    }

    //---------------手势识别检测 (轻量 MLP) --------------------
    if (cameraModule.isAvailable()) {
        unsigned long now = millis();
        if (now - cameraModule.getLastCaptureMs() >= cameraModule.getIntervalMs()) {
            if (cameraModule.captureGrayscale(gestureFrameBuffer)) {
                GestureNNResult nnResult = gestureNN.classify(gestureFrameBuffer);
                handleGestureNN(nnResult);
            }
        }
    }

    //---------------情感状态衰减 (每 1s) -----------------------
    emotionEngine.update();

    //---------------扬声器状态更新--------------------------
    speaker.loop();

    //---------------情感动作更新 (非阻塞)-------------------
    motionEmotion.loop();

    //---------------AI Agent 请求处理--------------------------
    // 检查是否有待处理的 AI 请求，且 Agent 空闲
    if (pendingAgentInput && agentState == AGENT_IDLE
        && millis() - agentCooldownMs > AGENT_COOLDOWN_MS) {

        agentState = AGENT_BUSY;
        pendingAgentInput = false;
        String textToProcess = pendingAgentText;
        activeAgentRequestId = pendingAgentRequestId;
        agentResultStatus = "processing";

        Serial.printf("AI: Processing request: \"%s\"\n", textToProcess.c_str());

        // 阻塞调用 (2-8 秒，AsyncWebServer 后台继续处理请求)
        AgentResponse response;
        bool success = aiAgent.ask(textToProcess, response);

        if (success) {
            dispatchAgentResponse(response);
            agentResultStatus = "completed";
            Serial.println("AI: Response dispatched successfully");
        } else {
            // 使用后备响应
            AgentResponse fallback = aiAgent.getFallbackResponse();
            dispatchAgentResponse(fallback);
            agentResultStatus = "failed";
            Serial.println("AI: Using fallback response");
        }

        agentState = AGENT_IDLE;
        agentCooldownMs = millis();
    }
    // 对 ADC 数据多次采样并求平均
    float adcVoltage = getAverageAdcVoltage();

    // 将采样的 ADC 电压转换为实际电池电压
    batteryVoltage = adcVoltage * voltageDividerRatio; // 计算电池电压

    // 根据电池电压计算电量百分比
    batteryPercentage = mapBatteryPercentage(batteryVoltage);

    if (buttonPressed)//按键1按下时
    {
        buttonPressed = false; // 清除按键标志
        front();
    }
    if (emojiState != prevEmojiState)
    {
        u8g2.clearDisplay();         // 状态变化时清屏
        prevEmojiState = emojiState; // 更新状态
    }
    if (freestate)
    {
        delay(3000);
        actionstate = random(0, 10);
    }
    // 可以使用switch优化效率
    // 注意: 当情感动作正在播放时，跳过普通动作避免冲突
    if (motionEmotion.isPlaying() && actionstate > 0 && actionstate <= 10) {
        // 情感动作优先，延后执行普通动作
        // 将 actionstate 保留，下个循环再试
    } else {
    switch (actionstate)
    {
    case 0 /* constant-expression */:
        /* code */
        break;
    case 1:
        front(); // 执行一次舵机动作
        speaker.play(MELODY_BEEP); // 动作完成音效
        actionstate = 0;
        break;
    case 2:
        left(); // 执行一次舵机动作
        speaker.play(MELODY_BEEP);
        actionstate = 0;
        break;
    case 3:
        right(); // 执行一次舵机动作
        speaker.play(MELODY_BEEP);
        actionstate = 0;
        break;
    case 4:
        back(); // 执行一次舵机动作
        speaker.play(MELODY_BEEP);
        actionstate = 0;
        break;
    case 5:
        toplefthand(); // 执行一次舵机动作
        speaker.play(MELODY_BEEP);
        actionstate = 0;
        break;
    case 6:
        toprighthand(); // 执行一次舵机动作
        speaker.play(MELODY_BEEP);
        actionstate = 0;
        break;

    case 10:
        dosleep(); // 执行一次舵机动作
        speaker.play(MELODY_DONE);
        actionstate = 0;
        break;
    case 7:
        lie(); // 执行一次舵机动作
        speaker.play(MELODY_DONE);
        actionstate = 0;
        break;
    case 8:
        sitdown(); // 执行一次舵机动作
        speaker.play(MELODY_DONE);
        actionstate = 0;
        break;
    case 9:
        emojiState = random(0, 7); // 执行一次舵机动作
        actionstate = 0;
        break;
    default:
        break;
    }
    }

    switch (emojiState)
    {
    case 0: // 首页
        u8g2.setFont(u8g2_font_ncenB14_tr);
        do
        {

            u8g2.drawXBMP(0, 0, 128, 64, hi);
        } while (u8g2.nextPage());

        break;
    case 1: // 第二页

        u8g2.setFont(u8g2_font_ncenB14_tr);
        do
        {

            u8g2.drawXBMP(0, 0, 128, 64, angry);
        } while (u8g2.nextPage());

        break;
    case 2: // 第三页
        do
        {
            u8g2.setFont(u8g2_font_ncenB14_tr);
            u8g2.drawXBMP(0, 0, 128, 64, error);
        } while (u8g2.nextPage());

        break;
    case 3: // 第四页
        do
        {
            u8g2.setFont(u8g2_font_ncenB14_tr);
            u8g2.drawXBMP(0, 0, 128, 64, dowhat);
        } while (u8g2.nextPage());

        break;
    case 4: // 第四页

        do
        {
            u8g2.setFont(u8g2_font_ncenB14_tr);
            u8g2.drawXBMP(0, 0, 128, 64, love);
        } while (u8g2.nextPage());
        break;
    case 5:

        do
        {
            u8g2.setFont(u8g2_font_ncenB14_tr);
            u8g2.drawXBMP(0, 0, 128, 64, sick);
        } while (u8g2.nextPage());
        break;
    case 6:
        do
        {
            u8g2.setFont(u8g2_font_ncenB14_tr);
            u8g2.drawXBMP(0, 0, 128, 64, yun);
        } while (u8g2.nextPage());

        break;
    case 7:
        if (WiFi.status() != WL_CONNECTED)
        {
            do
            {
                u8g2.setFont(u8g2_font_ncenB08_tr);
                u8g2.drawXBMP(0, 0, 64, 64, wifi);
                u8g2.drawStr(64, 20, "IP:");
                u8g2.drawStr(64, 40, "192.168.4.1");
                u8g2.drawStr(64, 60, "Need NET");
            } while (u8g2.nextPage());
        }
        else
        {
            fetchWeather();
        }
        break;

        break;
    case 8:
        if (WiFi.status() != WL_CONNECTED)
        {
            do
            {
                u8g2.setFont(u8g2_font_ncenB08_tr);
                u8g2.drawXBMP(0, 0, 64, 64, wifi);
                u8g2.drawStr(64, 20, "IP:");
                u8g2.drawStr(64, 40, "192.168.4.1");
                u8g2.drawStr(64, 60, "Need NET");
            } while (u8g2.nextPage());
        }
        else
        {
            do
            {
                timeClient.update(); // 更新时间
                u8g2.setFont(u8g2_font_ncenB14_tr);
                timeClient.update();
                u8g2.drawXBMP(0, 0, 64, 64, timeimage);
                // 获取当前时间
                // 显示时间到 OLED
                int currentHour = timeClient.getHours();
                int currentMinute = timeClient.getMinutes();
                String timeToDisplay = String(currentHour) + ":" + String(currentMinute);
                u8g2.drawStr(64, 30, "TIME");
                u8g2.setCursor(64, 50);
                u8g2.print(timeToDisplay);

            } while (u8g2.nextPage());
        }
        break;
    case 9:

        do
        {
            u8g2.setFont(u8g2_font_ncenB14_tr);
            u8g2.drawXBMP(0, (64 / 2 - 22 / 2), 128, 22, logo);
        } while (u8g2.nextPage());
        break;
    default:
        // 添加默认 case 来处理其他情况
        break;
    }


}
