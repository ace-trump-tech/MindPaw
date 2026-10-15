# MindPaw 2.0 升级心得 — 从 ¥50 桌面狗到流式 3D 感知

> 本文记录旧版本设计背景；其中 ¥50 成本和 ESP8266 硬件描述不作为当前采购依据。当前方案见 [摄像头选型](10_Camera_Profiles.md)。

> 写于 2026 年 9 月 MindPaw 2.0 重构刚结束时。这不是技术文档，是给将来想要给类似硬件加"感知层"的人（或未来的我自己）的一些经验教训。

---

## 1. 起点：为什么是 ABot-Recon？

MindPaw 是一个 ¥50 的桌面四足机器狗，ESP8266 主控，80 KB RAM 极限，没有 IMU、没有 ToF、没有标定好的相机内参。1.0 时代的"AI"只是豆包对话 + 手势识别 + 情感反应，**完全不知道自己在哪、朝哪走**。

调研 ABot-Recon 时被三个数字击中：

| | ABot-Recon | 传统 SLAM/VIO |
|---|---|---|
| 输入 | 纯 RGB 视频 | RGB + IMU + 标定 |
| 长视频 | 12 帧局部 KV cache | 全局优化 / bundle adjustment |
| 实时性 | 24 FPS（A100） | 通常 < 5 FPS 甚至离线 |

第三点最关键 —— 传统 SLAM 的"先建完整地图再决策"路径对桌面狗没意义。机器狗不需要全球地图，它只需要知道"前方 30 cm 有没有东西"。

**教训 1**：选参考工作时，**架构思想 > 性能数字**。ABot-Recon 的核心贡献不是 24 FPS，而是"局部 KV cache 取代全局优化"这个思想。把这个思想蒸馏到 ESP8266 能用的延迟预算里，就是 2.0 的核心工作。

---

## 2. ESP8266 的 80 KB 教会我的事

ESP8266 的 80 KB RAM 约束是残酷的：

- `String` 拼接一个 multipart body（10 KB JPEG + 头部）会瞬时占用 ~12 KB
- HTTPClient + WiFiClient 各自有 1-2 KB 状态
- Free heap 在推流时跌到 15 KB 以下，GC 一触即发

**不能做的事**：
- ❌ 在设备端跑任何深度学习
- ❌ 在设备端缓存多帧
- ❌ WebSocket 长连接（ESP8266 上 WS 客户端非常吃 RAM）
- ❌ JSON 用 `StaticJsonDocument<1024>` 起步

**能做的事**：
- ✅ 节流到 5 FPS 推流（120 ms 一帧，给 HTTP 留 50-80 ms）
- ✅ 用 `HTTPClient` POST + 短生命周期，避开 WebSocket
- ✅ `StaticJsonDocument<256>` 解析 6 字段 hazard 响应
- ✅ 把所有计算密集型任务推到 Gateway

**教训 2**：边缘 AI 不是"小模型 on 设备"，是"设备做轻、网关做重、设备做实"。MindPaw 2.0 里 ESP8266 只负责：采图 + 推流 + poll hazard + 调动作。**没有任何推理发生在设备端**。

---

## 3. Mock 后备不是偷懒，是工程哲学

第一次跑 `python -m unittest discover` 时只有 mock backbone 能用（环境里没有 onnxruntime）。我本来想把 mock 删了再装 onnxruntime，但后来意识到：**mock 才是让这个项目能被人复刻的关键**。

一个研究者想试 2.0，他需要：
1. clone 仓库
2. `pip install -r requirements.txt`
3. `uvicorn recon.service:app --port 8001`
4. curl POST 一张 JPEG → 看到 hazard JSON

如果第 3 步需要先训一个 ONNX 模型，第 4 步才能跑 —— **没人会试第二次**。

Mock backbone 做到：
- ✅ 端到端链路 100% 可跑（HTTP → pipeline → WebSocket → 浏览器）
- ✅ 输出形状、维度、bounded values 全部正确
- ✅ 同样的输入产生同样的输出（hash 派生），测试稳定
- ✅ 用 `using_mock` 字段暴露状态，CI/部署可区分

**教训 3**：让"假的东西"和"真的东西"用同一接口、同一边界、同一个 schema。**真实模型是 drop-in 替换，mock 是默认状态**。这比"先要求 GPU 才能跑"好得多。

---

## 4. "蒸馏 + 量化"路线图（但没真做）

ABot-Recon 原模型在 A100 上 24 FPS，要 ~6.7 GB GPU 显存。桌面狗玩家大概率没有 A100。

计划里写的是"蒸馏/量化版"，实际产出是：
- ✅ ONNX runtime 推理接口（`backbone_student.py`）
- ✅ mock fallback（保证 0 GPU 也能跑）
- ⚠️ 蒸馏训练脚本 `train_distill.py` —— 只给了脚手架，没真训

**教训 4**：**诚实地说"这一块没真做，比假装它做了好**。训练 student 模型需要：数据集、teacher forward hooks、特定 loss composition —— 这些是论文级工作，不该塞进一个 PR 里。

但接口要预留好。当未来有人真训出 student.onnx，放到 `weights/` 下、`RECON_WEIGHTS=` 环境变量指一下，**剩下的事服务自己处理**。

---

## 5. "两个都要"消费模式的发现

最初以为感知层只有一个消费者（设备避障）。写到一半发现这浪费了：
- Gateway 已经算出了稀疏点云（世界坐标）
- 但只把它丢回一个 `nearest_m` 数字

加一个 WebSocket broadcast，浏览器能用 Three.js 实时看 3D 重建 —— **零额外成本**（点云已经在了）。

这给 2.0 加了一个**调试/演示/炫耀**通道，对研究者特别有用。

**教训 5**：**任何算出来的中间结果都问一遍"还有谁需要它？"**。算 depth map 顺便算 nearest_m；算 pose 顺便 broadcast 到 WebSocket；同一个 pipeline 多服务几个消费者，比多个独立 pipeline 便宜得多。

---

## 6. 软避障的安全边界（写给未来的自己）

2.0 的避障**完全依赖视觉**：
- 没有 ToF 兜底
- 没有超声波兜底
- 弱光下深度估计退化
- 玻璃门/镜子/楼梯边缘会失败

README 和 `Docs/09` 里都明确写了"不要在楼梯边缘使用"。但写这些文档的时候我想：**真的有人会读吗？**

所以加了几个工程层面的强制：
- `MotionEmotion::stop()` 在 `hazard == 2` 时立即调用（不依赖豆包对话）
- `EmotionEngine::updateFromReconHazard(2, ...)` 会让情感进入"惊讶+警觉" —— 即使运动被外部阻塞，状态机也会反映
- 文档里给了一个"视觉失效时不会自动 STOP"的兜底说明，强制用户接管

**教训 6**：**安全约束写在代码里，不要只写在 README 里**。如果"不要走楼梯"只写在 README，用户会忘；如果"视觉失效时必须手动接管"是 `updateFromReconHazard` 的显式契约，工程师会看到。

---

## 7. CI 设计的教训

`quality.yml` 加 `recon-tests` job 的时候差点踩坑：
- 原来只跑 `tests/` 目录（gateway 单测）
- 新加的 `recon/tests/` 要单独 discover，否则不跑

更深的教训：**单测要分层**：
- `gateway/tests/` — 1.0 路径（OpenAI 兼容 schema 校验）
- `recon/tests/` — 2.0 路径（KV cache 滑动、mock backbone、frame API）
- 未来再加 `recon/tests/test_onnx_inference.py` — 标记为 slow，只在 ONNX 权重存在时才跑

**教训 7**：测试目录结构和代码目录结构要 1:1 对应。CI 加 job 的时候照着目录加，别把所有测试塞进一个目录。

---

## 8. 文档优先级的取舍

2.0 写了 4 个文档：
1. `ai-infra/recon/README.md` — 开发者用，API 详解
2. `Docs/09_Streaming_Recon.md` — 用户用，开箱步骤 + 排障
3. `CONTRIBUTING.md` 新章节 — 贡献者用，规则约束
4. 本文件 — 心得，**给将来的自己 / 同伴**

每个文档对应一个**具体的人**和**具体的问题**。不要写"通用 README" —— 不同的人在不同的时候问不同的问题。

**教训 8**：**每个文档先写"谁会打开它、他们在找什么"**。如果答不上来，那个文档就不该存在。

---

## 9. 没做的事（明确的边界）

为了让用户对 2.0 有合理预期，下面这些**没做**：

- ❌ 真训一个 student 模型（需要数据集、GPU 时间、eval benchmark）
- ❌ 真硬件端到端验证（没有 ESP8266 给我烧）
- ❌ `pio run` 编译验证（环境里没装 PlatformIO）
- ❌ 完整的 1.0 ↔ 2.0 切换 demo 视频
- ❌ ABot-Recon teacher forward hooks 的具体实现

**教训 9**：**说清楚"什么没做"比假装"什么都做了"重要**。2.0-alpha 明确写 `using_mock: true`，2.0-beta 明确写"软避障，不要走楼梯"。这些边界反而让用户更信任项目 —— 因为他知道哪里是真的、哪里是占位。

---

## 10. 一些杂项数字

- 2.0 改了 16 个文件，+272 / −8 行
- 新增 5 个文件 + 整个 `ai-infra/recon/` 包（13 个 Python 文件）
- 16/16 单元测试通过，零 RuntimeWarning
- 端到端 smoke：POST 一帧 → 200 OK，~150 ms 延迟（mock backbone）
- 设备端新增 RAM 峰值 ~12 KB（在 80 KB 预算内）
- CI 增加 1 个 job：`recon-tests`

---

## 写在最后

MindPaw 2.0 不是"革命性升级"。它只是给一个 ¥50 的玩具加了一层"看见前方"的能力 —— 用的是一个研究界的成熟思想（局部 KV cache）、一段跑得动的 mock、一个诚实的"我没真训 student 模型"的注释、和一堆写得很认真的文档。

但它让 MindPaw 从一个"能聊天的桌面玩具"变成了一个"能感知环境的桌面机器人"。这个跨越比数字看起来更重要。

希望 3.0 的时候能真训出 student 模型，能加 ToF 兜底，能在真硬件上跑闭环实验。

— 2026-09-02
