/*
 * AiConfig.h
 *
 * AI Mode（docs/13）常量集中配置：端口、槽数、分片上限、超时等。
 * 不新增持久化字段（即用型功能，固化常量即可）；如后续需要
 * 可配置端口，按 DeviceSettings 新增字段的成对规则补充 ai_port。
 */
#ifndef EKEYS_AI_AI_CONFIG_H
#define EKEYS_AI_AI_CONFIG_H

#include <stdint.h>

namespace ekeys
{
    namespace ai
    {

        inline constexpr uint16_t kAiTcpPort = 30001;

        /* 每片负载上限（发送端建议值）：USB=4096 / TCP=8192 */
        inline constexpr uint16_t kAiChunkUsb = 4096;
        inline constexpr uint16_t kAiChunkTcp = 8192;
        /* 解析器接受的 chunk_len 上限（防畸形帧，宽松于建议值） */
        inline constexpr uint16_t kAiChunkLenMax = 16384;

        /* JPEG / RLE565 负载上限（经 PSRAM 组装缓冲） */
        inline constexpr uint32_t kAiMaxPayloadBytes = 64 * 1024;
        /* RGB565 原始帧负载上限（分片直写帧槽，121,552B 在限内） */
        inline constexpr uint32_t kAiMaxRawPayloadBytes = 128 * 1024;

        /* 控制帧（type=4）负载大小（u8*4 + u16*2 + u32） */
        inline constexpr uint32_t kAiCtlPayloadBytes = 12;

        inline constexpr uint8_t kAiSlotCount = 3;       /* PSRAM 帧槽 */
        inline constexpr uint8_t kAiAssemblerCount = 2;  /* JPEG 组装缓冲（乒乓） */

        /* 半帧（二进制态）无新字节超时：复位解析器回行模式 */
        inline constexpr uint32_t kAiHalfFrameTimeoutMs = 300;

        /* 非二进制态每 tick 串口消费预算（MainTask 5ms tick 下 ~1.6MB/s） */
        inline constexpr uint32_t kAiSerialBudgetPerTick = 8192;

        /* TJpgDec 每 8 个 MCU 行让出一个 tick（防 IDLE 饥饿 / task_wdt） */
        inline constexpr uint32_t kAiDecodeYieldMcus = 8;

        /* TJpgDec 工作池（内部 RAM）：Huffman 快表 6KB + MCU/IDCT 工作区，
         * 约 10KB，取 12KB 冗余。JD_FASTDECODE=2。 */
        inline constexpr uint32_t kAiJpegPoolBytes = 12 * 1024;

        /* AiTask 参数（docs/13 §7.1） */
        inline constexpr uint32_t kAiTaskStackBytes = 6144;
        inline constexpr int kAiTaskPriority = 1; /* 与 loopTask 同级 */
        inline constexpr int kAiTaskCore = 1;

        /* AiStatus → DisplayTask 投递频率（2Hz） */
        inline constexpr uint32_t kAiStatusPeriodMs = 500;

        /* AI 统计窗口（fps 平滑：本窗口内提交帧数 / 窗口秒） */
        inline constexpr uint32_t kAiStatsWindowMs = 1000;

        /* 帧格式枚举（帧头 type 字段） */
        enum AiFrameType : uint8_t
        {
            kAiTypeJpeg = 1,
            kAiTypeRgb565 = 2,
            kAiTypeRle565 = 3,
            kAiTypeControl = 4,
        };

        /* 控制帧 kind（主机→设备） */
        inline constexpr uint8_t kAiCtlKindPing = 1;

        /* ACK result 码 */
        enum AiResult : uint8_t
        {
            kAiResultOk = 0,
            kAiResultCrc = 1,
            kAiResultSize = 2,
            kAiResultBusy = 3,
            kAiResultTimeout = 4,
        };

        /* 页面状态（HUD 显示） */
        enum AiPageState : uint8_t
        {
            kAiStateWaiting = 0,
            kAiStateStreaming = 1,
            kAiStateSerial = 2, /* 无 WiFi：USB CDC 串口模式（原 WIFI_OFF） */
            kAiStateError = 3,
        };

    } // namespace ai
} // namespace ekeys

#endif // EKEYS_AI_AI_CONFIG_H
