#include "ui/KeyMapPage.h"
#include "ui/Pages.h"
#include "ble/BleKeyMap.h"
#include <Arduino.h>
#include <stdio.h>

namespace ekeys
{

    /* 把库码（hidToLibKey 之后的 uint8_t）转换为短文本标签。
     * - 库码 < 0x80：ASCII 字符
     * - 库码 >= 0x80 且 < 0xE0：HID Usage ID = 库码 - 0x88，对照常见表给短串
     * - 库码 >= 0xE0：modifier 或 media，给固定符号
     * 输出最长 4 字符（含结尾 \0）。 */
    static void labelFromLibKey(uint8_t libKey, char *out, size_t outLen)
    {
        if (out == nullptr || outLen == 0)
            return;
        if (libKey == 0)
        {
            snprintf(out, outLen, "-");
            return;
        }

        /* ASCII 路径 */
        if (libKey < 0x80)
        {
            char c = (char)libKey;
            /* 部分 ASCII 是不可打印的控制字符；简单把它们转成 '?' */
            if (c < 0x20 || c == 0x7F)
                snprintf(out, outLen, "?");
            else
                snprintf(out, outLen, "%c", c);
            return;
        }

        /* media (Consumer Page 0x0C) 几个常用 key：库的 _asciimap 对 0x80..0x87
         * 区间特殊处理；这里常见的 media 键直接是消费页 ID。库码若落到这里
         * 我们按 HID Usage ID = libKey - 0x88 反推后给固定标签。 */
        uint8_t hid = (uint8_t)(libKey - 0x88U);

        /* 字母 a-z / A-Z */
        if (hid >= 0x04 && hid <= 0x1D)
        {
            snprintf(out, outLen, "%c", (char)('a' + (hid - 0x04)));
            return;
        }
        /* 数字 1-9, 0 */
        if (hid >= 0x1E && hid <= 0x26)
        {
            snprintf(out, outLen, "%d", hid - 0x1E + 1);
            return;
        }
        if (hid == 0x27) { snprintf(out, outLen, "0"); return; }

        /* 常见功能键 */
        switch (hid)
        {
        case 0x28: snprintf(out, outLen, "Ent"); return; // Enter
        case 0x29: snprintf(out, outLen, "Esc"); return; // Esc
        case 0x2A: snprintf(out, outLen, "Bsp"); return; // Backspace
        case 0x2B: snprintf(out, outLen, "Tab"); return; // Tab
        case 0x2C: snprintf(out, outLen, "Spc"); return; // Space
        case 0x4F: snprintf(out, outLen, ">");   return; // Right Arrow
        case 0x50: snprintf(out, outLen, "<");   return; // Left Arrow
        case 0x51: snprintf(out, outLen, "v");   return; // Down Arrow
        case 0x52: snprintf(out, outLen, "^");   return; // Up Arrow
        case 0x4A: snprintf(out, outLen, "Hm");  return; // Home
        case 0x4B: snprintf(out, outLen, "PgU"); return; // Page Up
        case 0x4C: snprintf(out, outLen, "Del"); return; // Delete Forward
        case 0x4D: snprintf(out, outLen, "End"); return; // End
        case 0x4E: snprintf(out, outLen, "PgD"); return; // Page Down
        }

        /* Media 键（HID Consumer 0xCD/0xE9/0xEA 等）直接透传 libKey（库内部按
         * media 路径处理）。0xCD = Play/Pause, 0xB5 = Next, 0xB6 = Prev,
         * 0xE9 = Vol+, 0xEA = Vol- */
        if (libKey == 0xCD) { snprintf(out, outLen, "Play"); return; }
        if (libKey == 0xB5) { snprintf(out, outLen, "Next"); return; }
        if (libKey == 0xB6) { snprintf(out, outLen, "Prev"); return; }
        if (libKey == 0xE9) { snprintf(out, outLen, "Vol+"); return; }
        if (libKey == 0xEA) { snprintf(out, outLen, "Vol-"); return; }

        /* 兜底：显示 hex 前缀 */
        snprintf(out, outLen, "%02X", hid);
    }

    void KeyMapPage::labelForKeyId(uint8_t keyId, char *out, size_t outLen)
    {
        if (keyId < 1 || keyId > 9)
        {
            snprintf(out, outLen, "-");
            return;
        }
        labelFromLibKey(BLE_KEY_MAP[keyId], out, outLen);
    }

    /* ---- 颜色 ---- */
    static inline lv_color_t cellNormalBg() { return lv_color_hex(0x182230); }
    static inline lv_color_t cellActiveBg() { return lv_color_hex(0x2A87FF); }
    static inline lv_color_t cellNormalFg() { return lv_color_hex(0xC0C8D4); }
    static inline lv_color_t cellActiveFg() { return lv_color_hex(0xFFFFFF); }

    /* 3×3 矩阵在屏幕上的位置参数（与 KeyScanConfig.h 的梯形布局保持视觉一致）。
     * 屏幕 428x142，可用区域 y=58..118（高度 60），x=8..420（宽度 412）。
     * 单元格：宽 ≈ 412/3 ≈ 137，高 ≈ 60/3 = 20（但每行多塞点空隙，留 18）。
     * 实际渲染 cell label 居中在格内；这里只用 (x0,y0,w,h) 算 9 个格子的中心。
     *
     * 物理键布局（KeyScanConfig.h::keyMap）：
     *   ROW0: COL0=1, COL1=2               → 1 2 . .
     *   ROW1: COL1=3, COL2=4, COL3=5       → . 3 4 5
     *   ROW2: COL0=6, COL1=7, COL2=8, COL3=9 → 6 7 8 9
     *
     * 视觉上我们把 9 个键画成规整的 3×3（按 keyId 排序），左到右、上到下：
     *   1 2 3
     *   4 5 6
     *   7 8 9
     * 视觉一致比"严格梯形"更易读，物理梯形只是给人操作的位置提示，不需要
     * 在 UI 里复刻。 */
    static constexpr int16_t MATRIX_X0 = 8;
    static constexpr int16_t MATRIX_Y0 = 60;
    static constexpr int16_t MATRIX_W = 412;
    static constexpr int16_t MATRIX_H = 60;
    static constexpr int16_t CELL_GAP = 4;

    static inline int16_t cellX(uint8_t i)
    {
        /* i = 0..8，对应 keyId = i+1 */
        const int16_t cellW = (MATRIX_W - CELL_GAP * 2) / 3;
        const int16_t x = MATRIX_X0 + (int16_t)((int32_t)i % 3) * (cellW + CELL_GAP);
        return x;
    }
    static inline int16_t cellY(uint8_t i)
    {
        const int16_t cellH = (MATRIX_H - CELL_GAP * 2) / 3;
        const int16_t y = MATRIX_Y0 + (int16_t)((int32_t)i / 3) * (cellH + CELL_GAP);
        return y;
    }
    static inline int16_t cellWpx() { return (MATRIX_W - CELL_GAP * 2) / 3; }
    static inline int16_t cellHpx() { return (MATRIX_H - CELL_GAP * 2) / 3; }

    KeyMapPage::KeyMapPage(BleKeyboardSink &ble)
        : Page(/*id=*/PAGE_KEYMAP, "KeyMap", lv_color_hex(0x4DA3FF)), ble_(ble)
    {
    }

    void KeyMapPage::buildUi()
    {
        lv_obj_t *root_obj = root();
        lv_obj_set_style_bg_opa(root_obj, LV_OPA_TRANSP, LV_PART_MAIN);

        /* 标题 */
        lv_obj_t *title = lv_label_create(root_obj);
        lv_label_set_text(title, "KeyMap");
        lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 4);

        /* 标题右侧 profile 文本 */
        profile_label_ = lv_label_create(root_obj);
        lv_obj_set_style_text_color(profile_label_, lv_color_hex(0x4DA3FF), LV_PART_MAIN);
        lv_obj_set_style_text_font(profile_label_, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(profile_label_, LV_ALIGN_TOP_RIGHT, -8, 8);

        /* 分割线 */
        lv_obj_t *line = lv_obj_create(root_obj);
        lv_obj_set_size(line, SCREEN_W_PX - 16, 1);
        lv_obj_set_style_bg_color(line, lv_color_hex(0x404040), LV_PART_MAIN);
        lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
        lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 28);

        /* 3x3 网格 */
        const int16_t w = cellWpx();
        const int16_t h = cellHpx();
        for (uint8_t i = 0; i < 9; ++i)
        {
            /* 背景 */
            lv_obj_t *bg = lv_obj_create(root_obj);
            lv_obj_remove_style_all(bg);
            lv_obj_set_size(bg, w, h);
            lv_obj_set_pos(bg, cellX(i), cellY(i));
            lv_obj_set_style_bg_color(bg, cellNormalBg(), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_radius(bg, 4, LV_PART_MAIN);
            cell_bg_[i + 1] = bg;

            /* 数字 + 标签：上面 keyId，下面短标签
             * 用 2 个 label 叠加（keyId 用 14pt 小字灰，映射标签用 14pt 白）。
             * 简洁起见合并到一个 label："1→A" 这种格式。 */
            lv_obj_t *lab = lv_label_create(root_obj);
            lv_obj_set_style_text_color(lab, cellNormalFg(), LV_PART_MAIN);
            lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, LV_PART_MAIN);
            /* 把 label 居中放在 cell 内 */
            lv_obj_align(lab, LV_ALIGN_CENTER, 0, 0);
            /* LVGL 8.3 label 默认以左上角为锚点居中到 cell，需要给 cell 同样的
             * 中心坐标。我们直接给绝对坐标 + 文本宽度补偿麻烦，所以采用：
             * 先 set 文本，再 set 坐标为 cell 中心，然后 update_layout 取宽高，
             * 再用 set_x 减半宽微调。 */
            char buf[8];
            snprintf(buf, sizeof(buf), "%u", i + 1);
            lv_label_set_text(lab, buf);
            lv_obj_set_pos(lab,
                           cellX(i) + w / 2 - 6,
                           cellY(i) + h / 2 - 7);
            cell_labels_[i + 1] = lab;
        }

        /* 底部 hint */
        lv_obj_t *hint = lv_label_create(root_obj);
        lv_label_set_text(hint, "KNOB pick  K2 profile  K1 back");
        lv_obj_set_style_text_color(hint, lv_color_hex(0x6B7280), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, -8, -4);

        refresh();
    }

    void KeyMapPage::teardownUi()
    {
        profile_label_ = nullptr;
        for (uint8_t i = 0; i < 10; ++i)
        {
            cell_labels_[i] = nullptr;
            cell_bg_[i] = nullptr;
        }
    }

    void KeyMapPage::onEnter()
    {
        /* 每次进入都同步：profile 可能已被 BLE 主循环在别处改过 */
        selectedKeyId_ = 1;
        refresh();
    }

    void KeyMapPage::onExit()
    {
    }

    void KeyMapPage::onEncoder(int8_t delta)
    {
        /* 旋钮：在 9 个按键之间循环选择。 */
        int16_t next = (int16_t)selectedKeyId_ + delta;
        if (next < 1)
            next = 9;
        else if (next > 9)
            next = 1;
        selectedKeyId_ = (uint8_t)next;
        refresh();
    }

    void KeyMapPage::onConfirm()
    {
        /* KEY2 / 旋钮按下 = 切到下一个 profile。 */
        uint8_t cur = ble_.activeProfile();
        uint8_t next = (cur + 1) % BLE_PROFILE_COUNT;
        ble_.setActiveProfile(next);
        refresh();
    }

    void KeyMapPage::onSelectKey(uint8_t keyId)
    {
        /* KEY3..KEY9 直接跳到对应 keyId。KEY1 由 PageManager 处理（back）。
         * KEY2 已经走 onConfirm。 */
        if (keyId >= 3 && keyId <= 9)
        {
            selectedKeyId_ = keyId;
            refresh();
        }
    }

    void KeyMapPage::refresh()
    {
        if (profile_label_ == nullptr)
            return;

        /* Profile 文本 */
        const KeyMapProfile &p = bleProfile(ble_.activeProfile());
        char profBuf[48];
        snprintf(profBuf, sizeof(profBuf), "[%u/%u] %s",
                 (unsigned)(ble_.activeProfile() + 1),
                 (unsigned)BLE_PROFILE_COUNT,
                 p.name);
        lv_label_set_text(profile_label_, profBuf);

        /* 矩阵单元：刷新背景色 + 文本 */
        for (uint8_t i = 1; i <= 9; ++i)
        {
            const bool active = (i == selectedKeyId_);
            if (cell_bg_[i] != nullptr)
                lv_obj_set_style_bg_color(cell_bg_[i],
                                          active ? cellActiveBg() : cellNormalBg(),
                                          LV_PART_MAIN);
            if (cell_labels_[i] != nullptr)
            {
                /* 重画文本为 "keyId→<label>" */
                char mapTxt[8];
                char lbl[5];
                labelForKeyId(i, lbl, sizeof(lbl));
                snprintf(mapTxt, sizeof(mapTxt), "%u>%s", i, lbl);
                lv_label_set_text(cell_labels_[i], mapTxt);
                lv_obj_set_style_text_color(cell_labels_[i],
                                            active ? cellActiveFg() : cellNormalFg(),
                                            LV_PART_MAIN);
            }
        }
    }

} // namespace ekeys