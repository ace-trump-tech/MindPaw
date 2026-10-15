//----------------------------------------------
// StreamingReconClient 实现 — 2.0 流式 3D 重建感知层客户端
//----------------------------------------------
#include "streaming_recon.h"
#include <ArduinoJson.h>

// ==================== 构造函数 ====================
StreamingReconClient::StreamingReconClient()
    : _baseUrl(""), _token(""), _fps(RECON_DEFAULT_FPS),
      _enabled(false), _streaming(false), _debug(false),
      _lastPushMs(0),
      _lastHazard(RECON_HAZARD_SAFE),
      _lastNearestM(10.0f),
      _lastDriftCm(0.0f),
      _lastFrameId(0) {
}

// ==================== 配置 ====================
void StreamingReconClient::configure(const String& baseUrl, const String& token) {
    _baseUrl = baseUrl;
    _token = token;
    _enabled = (baseUrl.length() > 0);
    if (_debug) {
        Serial.printf("RECON: configured base=%s enabled=%d\n",
                      _baseUrl.c_str(), _enabled);
    }
}

void StreamingReconClient::disable() {
    _enabled = false;
    _streaming = false;
    if (_debug) Serial.println("RECON: disabled");
}

// ==================== 节流 tick ====================
bool StreamingReconClient::tick(const uint8_t* jpegBytes, size_t jpegLen, unsigned long frameId) {
    if (!_enabled || !_streaming || !jpegBytes || jpegLen == 0) return false;
    if (jpegLen > RECON_MAX_JPEG_BYTES) {
        if (_debug) Serial.println("RECON: skip oversized frame");
        return false;
    }

    unsigned long now = millis();
    unsigned long interval = 1000UL / (unsigned long)_fps;
    if (now - _lastPushMs < interval) return false;
    _lastPushMs = now;

    bool ok = _postFrame(jpegBytes, jpegLen, frameId);
    if (ok) _lastFrameId = frameId;
    return ok;
}

// ==================== 控制流 ====================
bool StreamingReconClient::startStreaming() {
    if (!_enabled) return false;
    bool ok = _postControl("/recon/start");
    if (ok) _streaming = true;
    return ok;
}

bool StreamingReconClient::stopStreaming() {
    if (!_enabled) return false;
    bool ok = _postControl("/recon/stop");
    _streaming = false;
    return ok;
}

// ==================== POST 一帧 JPEG ====================
bool StreamingReconClient::_postFrame(const uint8_t* jpegBytes, size_t jpegLen, unsigned long frameId) {
    String boundary = _buildBoundary();
    String body = _buildMultipartBody(boundary, frameId, jpegBytes, jpegLen);

    String url = _baseUrl + "/recon/frame";
    _http.begin(_tcpClient, url);
    _http.setTimeout(RECON_DEFAULT_TIMEOUT_MS);
    if (_token.length() > 0) {
        _http.addHeader("Authorization", "Bearer " + _token);
    }
    _http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

    int code = _http.POST((uint8_t*)body.c_str(), body.length());
    String response = _http.getString();
    _http.end();

    if (code != 200) {
        if (_debug) Serial.printf("RECON: POST frame %lu failed (HTTP %d)\n", frameId, code);
        return false;
    }
    _parseHazardJson(response);
    return true;
}

bool StreamingReconClient::_postControl(const char* path) {
    String url = _baseUrl + String(path);
    _http.begin(_tcpClient, url);
    _http.setTimeout(RECON_DEFAULT_TIMEOUT_MS);
    if (_token.length() > 0) _http.addHeader("Authorization", "Bearer " + _token);
    int code = _http.POST("");
    _http.end();
    if (_debug) Serial.printf("RECON: POST %s -> HTTP %d\n", path, code);
    return (code == 200);
}

// ==================== 解析 hazard 响应 ====================
void StreamingReconClient::_parseHazardJson(const String& body) {
    // 期望字段: frame_id, ts_ms, nearest_m, hazard, pose_drift_cm, depth_mean_m
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        if (_debug) Serial.printf("RECON: hazard JSON parse failed: %s\n", err.c_str());
        return;
    }
    int h = doc["hazard"] | RECON_HAZARD_SAFE;
    if (h < 0) h = RECON_HAZARD_SAFE;
    if (h > 2) h = RECON_HAZARD_STOP;
    _lastHazard = h;
    _lastNearestM = doc["nearest_m"] | 10.0f;
    _lastDriftCm = doc["pose_drift_cm"] | 0.0f;
    if (_debug) {
        Serial.printf("RECON: hazard=%d nearest=%.2fm drift=%.1fcm\n",
                      _lastHazard, _lastNearestM, _lastDriftCm);
    }
}

// ==================== multipart 构造 ====================
String StreamingReconClient::_buildBoundary() {
    // ESP8266 上没有 snprintf("%08x", random()); 用 millis 派生足够随机
    unsigned long seed = millis() ^ ESP.getCycleCount();
    char buf[20];
    snprintf(buf, sizeof(buf), "----recon%08lx", seed);
    return String(buf);
}

String StreamingReconClient::_buildMultipartBody(const String& boundary,
                                                  unsigned long frameId,
                                                  const uint8_t* jpegBytes, size_t jpegLen) {
    // 这里我们用 String 拼装 multipart，注意 ESP8266 的 80 KB RAM 上限
    // QQVGA JPEG 通常 <10 KB；String 临时占用 ~10 KB 峰值，可接受
    String body;
    body.reserve(64 + jpegLen);
    body += "--";
    body += boundary;
    body += "\r\nContent-Disposition: form-data; name=\"frame_id\"\r\n\r\n";
    body += String(frameId);
    body += "\r\n--";
    body += boundary;
    body += "\r\nContent-Disposition: form-data; name=\"ts_ms\"\r\n\r\n";
    body += String(millis());
    body += "\r\n--";
    body += boundary;
    body += "\r\nContent-Disposition: form-data; name=\"fps_target\"\r\n\r\n";
    body += String(_fps);
    body += "\r\n--";
    body += boundary;
    body += "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"frame.jpg\"\r\n";
    body += "Content-Type: image/jpeg\r\n\r\n";

    // 写入 JPEG 字节 (用 c_str + 长度避免中间 buffer 复制)
    body.concat((const char*)jpegBytes, jpegLen);

    body += "\r\n--";
    body += boundary;
    body += "--\r\n";
    return body;
}
