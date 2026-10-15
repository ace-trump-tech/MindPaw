//----------------------------------------------
// 豆包 AI Agent — HTTP 客户端实现
// 调用 火山引擎/方舟 OpenAI 兼容接口
//----------------------------------------------
#include "doubao_agent.h"
#include <string.h>

// ==================== 构造函数 ====================
DoubaoAgent::DoubaoAgent() {
    _busy = false;
    _enabled = false;
    _historyCount = 0;
    _historyIndex = 0;
    _debug = false;
    _requestBuf[0] = '\0';
    _responseBuf[0] = '\0';
}

// ==================== 配置 ====================
void DoubaoAgent::configure(const String& apiKey, const String& endpointId,
                            const String& baseUrl) {
    _apiKey = apiKey;
    _endpointId = endpointId;
    _baseUrl = baseUrl;
    _baseUrl.trim();
    if (_baseUrl.length() == 0) {
        _baseUrl = String(DOUBAO_BASE_URL) + DOUBAO_API_PATH;
    }
    _enabled = (apiKey.length() > 0 && endpointId.length() > 0);
    _lastReplyText = "";

    if (_enabled) {
        Serial.println("DOUBAO: Agent configured successfully");
    } else {
        Serial.println("DOUBAO: Agent configuration incomplete (disabled)");
    }
}

bool DoubaoAgent::isConfigured() const {
    return _apiKey.length() > 0 && _endpointId.length() > 0;
}

// ==================== 网络检查 ====================
bool DoubaoAgent::checkNetwork() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("DOUBAO: WiFi not connected");
        return false;
    }
    return true;
}

// ==================== 构建请求体 ====================
bool DoubaoAgent::buildRequestBody(const String& userText) {
    // 计算所需 JSON 文档容量
    // system prompt ~1200 + history(3*200=600) + user(200) + overhead ~ 3000
    DynamicJsonDocument doc(3072);

    // --- messages 数组 ---
    JsonArray messages = doc.createNestedArray("messages");

    // 1. System prompt (从 PROGMEM 复制到临时缓冲区)
    char sysPromptBuf[1536];
    strcpy_P(sysPromptBuf, AGENT_SYSTEM_PROMPT);
    JsonObject sysMsg = messages.createNestedObject();
    sysMsg["role"] = "system";
    sysMsg["content"] = sysPromptBuf;

    // 2. 对话历史 (循环缓冲)
    for (uint8_t i = 0; i < _historyCount; i++) {
        // _historyIndex points to the next slot to overwrite. The oldest
        // entry is therefore one slot after it when the buffer is full.
        uint8_t oldest = (_historyCount == AGENT_MAX_HISTORY) ? _historyIndex : 0;
        uint8_t idx = (oldest + i) % AGENT_MAX_HISTORY;
        if (_history[idx].user.length() == 0) continue;

        JsonObject histUser = messages.createNestedObject();
        histUser["role"] = "user";
        histUser["content"] = _history[idx].user;

        JsonObject histAsst = messages.createNestedObject();
        histAsst["role"] = "assistant";
        histAsst["content"] = _history[idx].assistant;
    }

    // 3. 情感上下文注入 (Affective Prompt Injection)
    // 如果设置了情感上下文，在用户消息前添加情感标签
    String enhancedUserText = userText;
    if (_affectiveContext.length() > 0) {
        enhancedUserText = _affectiveContext + " [用户消息] " + userText;
        if (_debug) {
            Serial.printf("DOUBAO: Affective prompt: %s\n", enhancedUserText.c_str());
        }
    }

    // 4. 当前用户输入 (含情感上下文)
    JsonObject curMsg = messages.createNestedObject();
    curMsg["role"] = "user";
    curMsg["content"] = enhancedUserText;

    // --- model ---
    doc["model"] = _endpointId;

    // --- response_format ---
    JsonObject respFormat = doc.createNestedObject("response_format");
    respFormat["type"] = "json_object";

    // --- temperature ---
    doc["temperature"] = 0.7;

    // --- max_tokens ---
    doc["max_tokens"] = 256;

    // 序列化到静态缓冲区
    size_t len = serializeJson(doc, _requestBuf, AGENT_REQ_BUF_SIZE);
    if (len >= AGENT_REQ_BUF_SIZE - 1) {
        Serial.println("DOUBAO: Request buffer overflow!");
        return false;
    }

    if (len < 50) {
        Serial.println("DOUBAO: Request too short, build error?");
        return false;
    }

    Serial.printf("DOUBAO: Request built (%d bytes)\n", len);
    return true;
}

// ==================== 解析 API 响应 ====================
bool DoubaoAgent::parseApiResponse(const char* json, AgentResponse& response) {
    // 解析顶层 API 响应
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, json);

    if (error) {
        Serial.printf("DOUBAO: JSON parse error: %s\n", error.c_str());
        // 尝试在响应中查找并提取 JSON 对象 {...}
        const char* braceStart = strchr(json, '{');
        if (braceStart) {
            // 重新尝试解析从 { 开始的内容
            error = deserializeJson(doc, braceStart);
            if (!error) {
                goto parseContent;
            }
        }
        return false;
    }

parseContent:
    // 提取 choices[0].message.content
    const char* content = doc["choices"][0]["message"]["content"];
    if (!content) {
        Serial.println("DOUBAO: No content in response");
        return false;
    }

    Serial.printf("DOUBAO: Raw content: %s\n", content);

    // content 本身应该是 JSON 对象
    DynamicJsonDocument contentDoc(512);
    error = deserializeJson(contentDoc, content);

    if (error) {
        // 尝试从 content 内部提取 {...}
        const char* innerStart = strchr(content, '{');
        if (innerStart) {
            error = deserializeJson(contentDoc, innerStart);
        }
        if (error) {
            Serial.printf("DOUBAO: Content JSON parse error: %s\n", error.c_str());
            return false;
        }
    }

    // 提取字段
    response.reply_text = contentDoc["reply_text"] | "嗯？";
    response.action    = contentDoc["action"]    | -1;
    response.expression= contentDoc["expression"]| -1;
    response.melody    = contentDoc["melody"]    | -1;
    response.repeat    = contentDoc["repeat"]    | 0;
    response.emotion   = contentDoc["emotion"]   | 0;

    Serial.printf("DOUBAO: Parsed reply=\"%s\" action=%d expr=%d melody=%d repeat=%d emotion=%d\n",
        response.reply_text.c_str(),
        response.action, response.expression,
        response.melody, response.repeat, response.emotion);

    return true;
}

// ==================== 追加对话历史 ====================
void DoubaoAgent::appendHistory(const String& user, const String& assistant) {
    // 截断过长文本
    String userTrunc = user.substring(0, 100);
    String asstTrunc = assistant.substring(0, AGENT_MAX_REPLY_LEN);

    // 循环缓冲
    _history[_historyIndex].user = userTrunc;
    _history[_historyIndex].assistant = asstTrunc;
    _historyIndex = (_historyIndex + 1) % AGENT_MAX_HISTORY;
    if (_historyCount < AGENT_MAX_HISTORY) {
        _historyCount++;
    }
}

// ==================== 清除历史 ====================
void DoubaoAgent::clearHistory() {
    for (uint8_t i = 0; i < AGENT_MAX_HISTORY; i++) {
        _history[i].user = "";
        _history[i].assistant = "";
    }
    _historyCount = 0;
    _historyIndex = 0;
    Serial.println("DOUBAO: History cleared");
}

// ==================== 核心 API 调用 ====================
bool DoubaoAgent::ask(const String& userText, AgentResponse& response) {
    if (_busy) {
        Serial.println("DOUBAO: Busy, request rejected");
        return false;
    }

    if (!_enabled || !isConfigured()) {
        Serial.println("DOUBAO: Not configured");
        response = getFallbackResponse();
        return false;
    }

    if (!checkNetwork()) {
        response = getFallbackResponse();
        return false;
    }

    _busy = true;

    // 1. 构建请求体
    if (!buildRequestBody(userText)) {
        response = getFallbackResponse();
        _busy = false;
        return false;
    }

    // 2. 发送 HTTP POST (带重试)
    bool httpSuccess = false;
    int httpCode = -1;
    String fullUrl = _baseUrl;

    for (int retry = 0; retry <= AGENT_MAX_RETRIES; retry++) {
        if (retry > 0) {
            Serial.printf("DOUBAO: Retry %d...\n", retry);
            delay(1000);
        }

        // ESP8266 cannot keep a CA bundle cheaply. The gateway deployment is
        // preferred; direct mode uses TLS with certificate verification
        // disabled for compatibility with existing boards.
        _client.setInsecure();
        if (!_http.begin(_client, fullUrl)) {
            Serial.println("DOUBAO: HTTP client begin failed");
            continue;
        }
        _http.setTimeout(AGENT_HTTP_TIMEOUT);
        _http.addHeader("Content-Type", "application/json");
        _http.addHeader("Authorization", "Bearer " + _apiKey);

        httpCode = _http.POST((uint8_t*)_requestBuf, strlen(_requestBuf));

        if (httpCode == 200) {
            // 成功
            String respBody = _http.getString();
            size_t respLen = respBody.length();
            if (respLen >= AGENT_RESP_BUF_SIZE) {
                Serial.printf("DOUBAO: Response truncated (%d bytes)\n", respLen);
                respLen = AGENT_RESP_BUF_SIZE - 1;
            }
            memcpy(_responseBuf, respBody.c_str(), respLen);
            _responseBuf[respLen] = '\0';
            httpSuccess = true;
            _http.end();
            Serial.printf("DOUBAO: HTTP 200, response %d bytes\n", respLen);
            break;
        }

        // 失败
        Serial.printf("DOUBAO: HTTP error %d\n", httpCode);
        _http.end();
    }

    if (!httpSuccess) {
        Serial.println("DOUBAO: All retries failed");
        response = getFallbackResponse();
        _busy = false;
        return false;
    }

    // 3. 解析 JSON 响应
    bool parseSuccess = parseApiResponse(_responseBuf, response);

    if (parseSuccess) {
        _lastReplyText = response.reply_text;
        appendHistory(userText, response.reply_text);
        Serial.println("DOUBAO: API call completed successfully");
    } else {
        Serial.println("DOUBAO: Parse failed, using fallback");
        response = getFallbackResponse();
    }

    _busy = false;
    return parseSuccess;
}

// ==================== 离线后备响应 ====================
AgentResponse DoubaoAgent::getFallbackResponse() {
    // 随机选择一个后备响应
    static const struct {
        const char* reply;
        int8_t action;
        int8_t expression;
        int8_t melody;
        int8_t emotion;
    } fallbacks[] = {
        {"汪汪！网络好像断了…", 3, 2, 2, 2},
        {"我的大脑掉线了，请稍后再试~", 3, 3, 2, 4},
        {"信号不太好，你说啥？", 3, 2, 7, 4},
        {"现在无法思考，待会再聊！", 10, 6, 2, 2},
    };

    static uint8_t fbIndex = 0;
    const auto& fb = fallbacks[fbIndex % 4];
    fbIndex++;

    AgentResponse resp;
    resp.reply_text = String(fb.reply);
    resp.action = fb.action;
    resp.expression = fb.expression;
    resp.melody = fb.melody;
    resp.repeat = 1;
    resp.emotion = fb.emotion;
    return resp;
}
