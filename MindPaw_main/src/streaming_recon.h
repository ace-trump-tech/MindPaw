//----------------------------------------------
// StreamingReconClient — 2.0 流式 3D 重建感知层客户端
//
// 复用 1.0 的 WiFiClientSecure + ESP8266HTTPClient 模式 (见 doubao_agent.h)，
// 但走裸 HTTP (multipart/form-data) 把 OV2640 JPEG 帧推到本地 ai-infra
// 网关的 /recon/frame 端点，并把 /recon/hazard 的最新避障决策缓存到
// _lastHazard，由 EmotionEngine::updateFromReconHazard 消费。
//
// 不动 1.0 的任何路径。默认 disable，用户在 aiconfig.html 填 Recon
// Gateway URL 后才启用。
//----------------------------------------------
#ifndef STREAMING_RECON_H
#define STREAMING_RECON_H

#include <Arduino.h>
#include "platform_network.h"
#include <WiFiClient.h>

// 默认推流配置
#define RECON_DEFAULT_FPS         5
#define RECON_DEFAULT_TIMEOUT_MS  1500
#define RECON_MAX_JPEG_BYTES      20000
#define RECON_HAZARD_SAFE         0
#define RECON_HAZARD_CAUTION      1
#define RECON_HAZARD_STOP         2

class StreamingReconClient {
public:
    StreamingReconClient();

    // 配置目标网关 URL (例如 "http://192.168.4.2:8001") + bearer token
    // baseUrl 不含路径；service 路径在内部追加
    void configure(const String& baseUrl, const String& token = "");
    void disable();

    bool isEnabled() const { return _enabled; }
    bool isConfigured() const { return _baseUrl.length() > 0; }
    bool isStreaming() const { return _streaming; }
    int  getTargetFps() const { return _fps; }
    void setTargetFps(int fps) { _fps = constrain(fps, 1, 20); }

    // 在 loop() 中周期调用：推一帧 + 更新 _lastHazard
    // 内部按 fps 节流；调用方无需自己 sleep
    // jpegBytes / jpegLen 来自 OV2640_Camera::captureJpeg
    // 返回: true=本帧推送成功
    bool tick(const uint8_t* jpegBytes, size_t jpegLen, unsigned long frameId);

    // 控制流 (POST /recon/start, /recon/stop)
    bool startStreaming();
    bool stopStreaming();

    // 缓存的最近 hazard 决策 (供 emotion_engine / 显示查询)
    int   getLastHazard() const { return _lastHazard; }
    float getLastNearestM() const { return _lastNearestM; }
    float getLastDriftCm() const { return _lastDriftCm; }
    unsigned long getLastFrameId() const { return _lastFrameId; }

    void setDebug(bool debug) { _debug = debug; }

private:
    String _baseUrl;          // e.g. "http://192.168.4.2:8001"
    String _token;            // bearer token (留空=关闭鉴权)
    int    _fps;
    bool   _enabled;
    bool   _streaming;
    bool   _debug;

    unsigned long _lastPushMs;

    // 缓存最近一次 hazard 决策
    int   _lastHazard;
    float _lastNearestM;
    float _lastDriftCm;
    unsigned long _lastFrameId;

    // 复用 HTTP client (避免反复构造)
    WiFiClient     _tcpClient;
    HTTPClient     _http;

    // multipart 上传 + 解析 hazard 响应
    bool _postFrame(const uint8_t* jpegBytes, size_t jpegLen, unsigned long frameId);
    bool _postControl(const char* path);
    void _parseHazardJson(const String& body);

    static String _buildBoundary();
    String _buildMultipartBody(const String& boundary,
                                      unsigned long frameId,
                                      const uint8_t* jpegBytes, size_t jpegLen);
};

#endif // STREAMING_RECON_H
