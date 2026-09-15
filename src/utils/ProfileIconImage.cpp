/*
 * ProfileIconImage.cpp — 见头文件说明。
 *
 * 注意：本模块在 DisplayTask 上下文读 SPIFFS。MainTask（0x11 上传/清除）
 * 理论上可能并发写 SPIFFS，但两路径都是低频操作且 SPIFFS 文件级损坏
 * 只会导致解码失败 → 符号回退，无崩溃风险。
 */

#include "ProfileIconImage.h"

#include <Arduino.h>
#include <SPIFFS.h>
#include <stdlib.h>
#include <string.h>

#include "config/Configuration.h"
#include "logging/LogManager.h"

/* lodepng 由 LV_USE_PNG=1 链入（lvgl/src/extra/libs/png/lodepng.c，C 编译）。
 * 不能直接 include lodepng.h：它既没有 extern "C" 包裹（C++ 侧会按 mangling
 * 名找符号导致链接失败），又含 #ifdef __cplusplus 的 std::vector 重载
 * （整头手动包 extern "C" 会报 conflicting declaration of C function）。
 * 这里只手动声明实际用到的两个 C 接口，签名与 lodepng.h 一致。 */
extern "C"
{
    unsigned lodepng_decode32(unsigned char **out, unsigned *w, unsigned *h,
                              const unsigned char *in, unsigned insize);
    const char *lodepng_error_text(unsigned code);
}

namespace ekeys
{

    namespace
    {

        constexpr size_t kMaxPngFileSize = 16 * 1024;
        constexpr uint16_t kMaxIconDim = 64;

        /*
         * RGBA8888 → LVGL 16bpp TRUE_COLOR_ALPHA 交错格式。
         * RGB565: (r>>3)<<11 | (g>>2)<<5 | b>>3，小端存储
         * （LV_COLOR_16_SWAP=0，lv_color16_t = blue:5 green:6 red:5），
         * alpha 原样拷贝。
         */
        bool convertRgbaToTrueColorAlpha(const uint8_t *rgba, uint16_t w, uint16_t h,
                                         uint8_t **out, size_t *out_size)
        {
            const size_t px_count = static_cast<size_t>(w) * h;
            const size_t total = px_count * 3; /* RGB565(2) + A8(1) 交错 */
            uint8_t *buf = static_cast<uint8_t *>(malloc(total));
            if (buf == nullptr)
            {
                return false;
            }
            for (size_t i = 0; i < px_count; ++i)
            {
                const uint8_t r = rgba[i * 4 + 0];
                const uint8_t g = rgba[i * 4 + 1];
                const uint8_t b = rgba[i * 4 + 2];
                const uint16_t rgb565 = (static_cast<uint16_t>(r >> 3) << 11) |
                                        (static_cast<uint16_t>(g >> 2) << 5) |
                                        (b >> 3);
                buf[i * 3 + 0] = static_cast<uint8_t>(rgb565 & 0xFF);
                buf[i * 3 + 1] = static_cast<uint8_t>(rgb565 >> 8);
                buf[i * 3 + 2] = rgba[i * 4 + 3];
            }
            *out = buf;
            *out_size = total;
            return true;
        }

    } // namespace

    bool profileIconLoadTrueColorAlpha(uint8_t profile,
                                       uint8_t **out_pixels,
                                       size_t *out_size,
                                       uint16_t *out_w,
                                       uint16_t *out_h)
    {
        if (out_pixels == nullptr || out_size == nullptr ||
            out_w == nullptr || out_h == nullptr)
        {
            return false;
        }

        const char *path = Configuration::instance().getProfileIconPath(profile);
        if (!SPIFFS.exists(path))
        {
            return false;
        }

        File f = SPIFFS.open(path, FILE_READ);
        if (!f)
        {
            return false;
        }
        const size_t file_size = f.size();
        if (file_size == 0 || file_size > kMaxPngFileSize)
        {
            f.close();
            LOG_WARNING("ICON", "%s size %u exceeds limit %u", path,
                        static_cast<unsigned>(file_size),
                        static_cast<unsigned>(kMaxPngFileSize));
            return false;
        }

        uint8_t *png = static_cast<uint8_t *>(malloc(file_size));
        if (png == nullptr)
        {
            f.close();
            return false;
        }
        const size_t read_len = f.read(png, file_size);
        f.close();
        if (read_len != file_size)
        {
            free(png);
            return false;
        }

        uint8_t *rgba = nullptr;
        unsigned w = 0;
        unsigned h = 0;
        const unsigned rc = lodepng_decode32(&rgba, &w, &h, png, file_size);
        free(png);
        if (rc != 0 || rgba == nullptr || w == 0 || h == 0 ||
            w > kMaxIconDim || h > kMaxIconDim)
        {
            if (rc != 0)
            {
                LOG_WARNING("ICON", "decode %s failed rc=%u (%s)", path, rc,
                            lodepng_error_text(rc));
            }
            else
            {
                LOG_WARNING("ICON", "%s dims %ux%u exceeds limit %u", path, w, h,
                            static_cast<unsigned>(kMaxIconDim));
            }
            free(rgba);
            return false;
        }

        bool ok = convertRgbaToTrueColorAlpha(rgba, static_cast<uint16_t>(w),
                                              static_cast<uint16_t>(h),
                                              out_pixels, out_size);
        free(rgba);
        if (ok)
        {
            *out_w = static_cast<uint16_t>(w);
            *out_h = static_cast<uint16_t>(h);
        }
        return ok;
    }

} // namespace ekeys
