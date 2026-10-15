//----------------------------------------------
// 豆包 AI Agent — HTTP 客户端
// 通过 HTTPS 调用 火山引擎/方舟 推理 API
//----------------------------------------------
#ifndef DOUBAO_AGENT_H
#define DOUBAO_AGENT_H

#include <Arduino.h>
#include "platform_network.h"
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "doubao_config.h"

class DoubaoAgent {
public:
    DoubaoAgent();

    // ---------- 配置 ----------
    void configure(const String& apiKey, const String& endpointId,
                   const String& baseUrl = "");
    bool isConfigured() const;
    const String& getLastReplyText() const { return _lastReplyText; }
    const String& getBaseUrl() const { return _baseUrl; }

    // ---------- 核心 API 调用 (阻塞 2-8 秒，在 loop 中调用) ----------
    // userText: 用户输入文本
    // response: 输出解析后的 AgentResponse
    // 返回: true=成功, false=失败(使用后备响应)
    bool ask(const String& userText, AgentResponse& response);

    // ---------- 状态 ----------
    bool isBusy() const { return _busy; }
    void setBusy(bool b) { _busy = b; }

    // ---------- 对话管理 ----------
    void clearHistory();
    void setEnabled(bool en) { _enabled = en; }
    bool isEnabled() const { return _enabled; }

    // ---------- 情感增强 ----------
    // 设置情感上下文 (由 MultimodalFusion 生成，注入到 LLM prompt)
    void setAffectiveContext(const String& context) { _affectiveContext = context; }
    void clearAffectiveContext() { _affectiveContext = ""; }
    const String& getAffectiveContext() const { return _affectiveContext; }

    // ---------- 调试 ----------
    void setDebug(bool debug) { _debug = debug; }

    // ---------- 后备响应 ----------
    AgentResponse getFallbackResponse();

private:
    String _apiKey;
    String _endpointId;
    String _baseUrl;
    bool _busy;
    bool _enabled;
    String _lastReplyText;

    // 情感上下文 (注入到每个请求)
    String _affectiveContext;
    bool _debug;

    // 对话历史 — 循环缓冲
    ConversationEntry _history[AGENT_MAX_HISTORY];
    uint8_t _historyCount;   // 当前有效条目数
    uint8_t _historyIndex;   // 下一个覆盖位置 (循环)

    // 复用静态缓冲区 (避免 String 堆碎片)
    char _requestBuf[AGENT_REQ_BUF_SIZE];
    char _responseBuf[AGENT_RESP_BUF_SIZE];

    // 复用 HTTP 客户端
    WiFiClientSecure _client;
    HTTPClient _http;

    // ---------- 内部方法 ----------
    bool buildRequestBody(const String& userText);
    bool parseApiResponse(const char* json, AgentResponse& response);
    void appendHistory(const String& user, const String& assistant);
    bool checkNetwork();
};

#endif // DOUBAO_AGENT_H
