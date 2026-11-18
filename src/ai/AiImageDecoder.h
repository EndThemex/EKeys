/*
 * AiImageDecoder.h
 *
 * AI Mode 图像解码（docs/13 §5）：JPEG（TJpgDec）/ RGB565 直通 / RLE565 →
 * PSRAM 帧槽。仅在 AiTask 上下文调用（禁止碰 LVGL）。
 */
#ifndef EKEYS_AI_AI_IMAGE_DECODER_H
#define EKEYS_AI_AI_IMAGE_DECODER_H

#include <stdint.h>

namespace ekeys
{
    namespace ai
    {

        enum class AiDecodeResult : uint8_t
        {
            Ok = 0,
            BadSize = 1, /* JPEG 实际尺寸与期望不符 */
            Failed = 2,  /* 解码错误（数据损坏/内存不足） */
        };

        /*
         * 解码 JPEG 到 RGB565 帧槽（stride = dst_w）。
         * scale_half：源为 856×284，按 1/2 输出 428×142（TJpgDec scale=1，
         * 跳过高频 IDCT，文字/线条更锐利，docs/13 §5.1）。
         * 解码期间每 kAiDecodeYieldMcus 个 MCU 行 vTaskDelay(1) 让出。
         */
        AiDecodeResult aiDecodeJpeg(const uint8_t *src, uint32_t len,
                                    uint8_t *dst, uint16_t dst_w, uint16_t dst_h,
                                    bool scale_half);

        /*
         * 解码 RLE565 到帧槽：PackBits 风格 [run_len u16][pixel u16]，
         * run_len ≥ 1；写满 dst_bytes 即止，源耗尽视为截断（返回 Ok，
         * 未写区域保留槽内旧像素由发送端保证填充完整）。
         */
        AiDecodeResult aiDecodeRle565(const uint8_t *src, uint32_t len,
                                      uint8_t *dst, uint32_t dst_bytes);

    } // namespace ai
} // namespace ekeys

#endif // EKEYS_AI_AI_IMAGE_DECODER_H
