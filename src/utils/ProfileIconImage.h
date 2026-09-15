/*
 * ProfileIconImage.h
 *
 * Profile 自定义图标（0x11 上传到 SPIFFS 的 /iconN.png）解码工具：
 * 读文件 → lodepng 解码 RGBA8888 → 转成 LVGL 8 16bpp
 * LV_IMG_CF_TRUE_COLOR_ALPHA 交错格式（每像素 RGB565 低 2 字节 +
 * alpha 高 1 字节，LV_COLOR_16_SWAP=0，见 lv_img_buf.c 的像素寻址）。
 *
 * 尺寸上限（防止 App 上传大图拖垮 DisplayTask 渲染帧 / OOM）：
 *   - 文件 ≤ 16KB（48×48 图标实测几 KB）
 *   - 解码尺寸 ≤ 64×64（展示位仅 50×50 徽章与 19px 底栏缩略）
 * 超限返回 false，调用方走符号回退。
 */

#ifndef EKEYS_UTILS_PROFILE_ICON_IMAGE_H
#define EKEYS_UTILS_PROFILE_ICON_IMAGE_H

#include <stddef.h>
#include <stdint.h>

namespace ekeys
{

    /*
     * 加载并解码指定 profile 的图标。
     * 成功返回 true，*out_pixels 为 malloc 缓冲（调用方 free），尺寸
     * *out_size = w*h*3（TRUE_COLOR_ALPHA 16bpp）。
     * 失败（无文件 / 超限 / 解码错误 / 内存不足）返回 false 且不写出参。
     */
    bool profileIconLoadTrueColorAlpha(uint8_t profile,
                                       uint8_t **out_pixels,
                                       size_t *out_size,
                                       uint16_t *out_w,
                                       uint16_t *out_h);

} // namespace ekeys

#endif // EKEYS_UTILS_PROFILE_ICON_IMAGE_H
