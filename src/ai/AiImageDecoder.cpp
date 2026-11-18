/*
 * AiImageDecoder.cpp
 *
 * 见 AiImageDecoder.h。TJpgDec（vendored lib/tjpgd，JD_FORMAT=1 直出
 * RGB565 + JD_FASTDECODE=2 Huffman 快表）：解码工作池 12KB 内部 RAM
 * （docs/13 §7.4），首次使用时分配并常驻，避免反复分配碎片化。
 *
 * 线程模型：只允许 AiTask（Core1）调用；输出回调直接写 PSRAM 帧槽，
 * 每 kAiDecodeYieldMcus 个 MCU 行让出一个 tick。
 */
#include "AiImageDecoder.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <string.h>

#include "AiConfig.h"
#include "logging/LogManager.h"
#include "tjpgd.h"

namespace ekeys
{
    namespace ai
    {

        namespace
        {

            /* TJpgDec 的 jd->device 是 inFunc / outFunc 共用的同一个指针，
             * 输入上下文与输出目标必须放在同一结构里。分开定义会导致
             * outFunc 把 JpegIn 强转成 JpegOutCtx，解码结果写到 JPEG 源
             * 缓冲（帧槽永远没有像素 → 花屏）。 */
            struct JpegCtx
            {
                /* 输入源（内存缓冲） */
                const uint8_t *data;
                uint32_t len;
                uint32_t pos;
                /* 输出目标 */
                uint8_t *dst;
                uint16_t w;
                uint32_t mcu_rows; /* 已输出的 MCU 行数（让出节拍） */
            };

            size_t inFunc(JDEC *jd, uint8_t *buf, size_t n)
            {
                auto *in = static_cast<JpegCtx *>(jd->device);
                if (in->pos >= in->len)
                {
                    return 0;
                }
                if (n > (in->len - in->pos))
                {
                    n = in->len - in->pos;
                }
                if (buf != nullptr)
                {
                    memcpy(buf, in->data + in->pos, n);
                }
                in->pos += n;
                return n;
            }

            int outFunc(JDEC *jd, void *bitmap, JRECT *rect)
            {
                auto *ctx = static_cast<JpegCtx *>(jd->device);
                auto *px = static_cast<uint16_t *>(bitmap);

                for (uint16_t y = rect->top; y <= rect->bottom; ++y)
                {
                    const uint32_t dst_off =
                        static_cast<uint32_t>(y) * ctx->w + rect->left;
                    const uint16_t row_w = rect->right - rect->left + 1;
                    memcpy(ctx->dst + dst_off * 2,
                           px + (y - rect->top) * row_w,
                           static_cast<size_t>(row_w) * 2);
                }

                /* TJpgDec 输出以 MCU 行为单位回调：每 kAiDecodeYieldMcus 行让出 */
                const uint16_t mcu_h = static_cast<uint16_t>(jd->msy) * 8;
                static uint16_t last_top = 0;
                if (rect->top != last_top)
                {
                    last_top = rect->top;
                    ++ctx->mcu_rows;
                    if (ctx->mcu_rows % kAiDecodeYieldMcus == 0)
                    {
                        vTaskDelay(1);
                    }
                }
                (void)mcu_h;
                return 1; /* 继续解码 */
            }

            /* TJpgDec 工作池（内部 RAM 12KB，常驻） */
            void *s_jpeg_pool = nullptr;

        } // namespace

        AiDecodeResult aiDecodeJpeg(const uint8_t *src, uint32_t len,
                                    uint8_t *dst, uint16_t dst_w, uint16_t dst_h,
                                    bool scale_half)
        {
            if (s_jpeg_pool == nullptr)
            {
                s_jpeg_pool = heap_caps_malloc(kAiJpegPoolBytes,
                                               MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
                if (s_jpeg_pool == nullptr)
                {
                    LOG_ERROR("AI", "jpeg pool alloc failed (%u B)",
                              static_cast<unsigned>(kAiJpegPoolBytes));
                    return AiDecodeResult::Failed;
                }
            }

            JDEC jd;
            JpegCtx ctx{src, len, 0, dst, dst_w, 0};
            const JRESULT prep = jd_prepare(&jd, inFunc, s_jpeg_pool,
                                            kAiJpegPoolBytes, &ctx);
            if (prep != JDR_OK)
            {
                LOG_WARNING("AI", "jd_prepare failed: %d", static_cast<int>(prep));
                return AiDecodeResult::Failed;
            }

            /* 尺寸校验：428×142 或 856×284（scale_half） */
            const uint16_t expect_w = scale_half ? dst_w * 2 : dst_w;
            const uint16_t expect_h = scale_half ? dst_h * 2 : dst_h;
            if (jd.width != expect_w || jd.height != expect_h)
            {
                LOG_WARNING("AI", "jpeg size %ux%u != %ux%u",
                            static_cast<unsigned>(jd.width),
                            static_cast<unsigned>(jd.height),
                            static_cast<unsigned>(expect_w),
                            static_cast<unsigned>(expect_h));
                return AiDecodeResult::BadSize;
            }

            const uint8_t scale = scale_half ? 1 : 0;
            const JRESULT rc = jd_decomp(&jd, outFunc, scale);
            if (rc != JDR_OK)
            {
                LOG_WARNING("AI", "jd_decomp failed: %d", static_cast<int>(rc));
                return AiDecodeResult::Failed;
            }
            return AiDecodeResult::Ok;
        }

        AiDecodeResult aiDecodeRle565(const uint8_t *src, uint32_t len,
                                      uint8_t *dst, uint32_t dst_bytes)
        {
            uint32_t sp = 0;
            uint32_t dp = 0;
            while (sp + 4 <= len && dp + 2 <= dst_bytes)
            {
                const uint16_t run =
                    static_cast<uint16_t>(src[sp]) |
                    static_cast<uint16_t>(src[sp + 1] << 8);
                if (run == 0)
                {
                    return AiDecodeResult::Failed; /* 非法 run */
                }
                const uint8_t *pixel = src + sp + 2;
                for (uint16_t i = 0; i < run && dp + 2 <= dst_bytes; ++i)
                {
                    dst[dp] = pixel[0];
                    dst[dp + 1] = pixel[1];
                    dp += 2;
                }
                sp += 4;
            }
            return AiDecodeResult::Ok;
        }

    } // namespace ai
} // namespace ekeys
