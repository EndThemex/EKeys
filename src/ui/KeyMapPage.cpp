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
        if (hid == 0x27)
        {
            snprintf(out, outLen, "0");
            return;
        }

        /* 常见功能键 */
        switch (hid)
        {
        case 0x28:
            snprintf(out, outLen, "Ent");
            return; // Enter
        case 0x29:
            snprintf(out, outLen, "Esc");
            return; // Esc
        case 0x2A:
            snprintf(out, outLen, "Bsp");
            return; // Backspace
        case 0x2B:
            snprintf(out, outLen, "Tab");
            return; // Tab
        case 0x2C:
            snprintf(out, outLen, "Spc");
            return; // Space
        case 0x4F:
            snprintf(out, outLen, ">");
            return; // Right Arrow
        case 0x50:
            snprintf(out, outLen, "<");
            return; // Left Arrow
        case 0x51:
            snprintf(out, outLen, "v");
            return; // Down Arrow
        case 0x52:
            snprintf(out, outLen, "^");
            return; // Up Arrow
        case 0x4A:
            snprintf(out, outLen, "Hm");
            return; // Home
        case 0x4B:
            snprintf(out, outLen, "PgU");
            return; // Page Up
        case 0x4C:
            snprintf(out, outLen, "Del");
            return; // Delete Forward
        case 0x4D:
            snprintf(out, outLen, "End");
            return; // End
        case 0x4E:
            snprintf(out, outLen, "PgD");
            return; // Page Down
        }

        /* Media 键（HID Consumer 0xCD/0xE9/0xEA 等）直接透传 libKey（库内部按
         * media 路径处理）。0xCD = Play/Pause, 0xB5 = Next, 0xB6 = Prev,
         * 0xE9 = Vol+, 0xEA = Vol- */
        if (libKey == 0xCD)
        {
            snprintf(out, outLen, "Play");
            return;
        }
        if (libKey == 0xB5)
        {
            snprintf(out, outLen, "Next");
            return;
        }
        if (libKey == 0xB6)
        {
            snprintf(out, outLen, "Prev");
            return;
        }
        if (libKey == 0xE9)
        {
            snprintf(out, outLen, "Vol+");
            return;
        }
        if (libKey == 0xEA)
        {
            snprintf(out, outLen, "Vol-");
            return;
        }

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

    /* 3×3 矩阵在屏幕上的位置参数。
     * 屏幕 428x142：
     *   y=58..122 高度 64；x=8..420 宽度 412。
     *   cellW = (412 - 4*2) / 3 = 134
     *   cellH = (64  - 4*2) / 3 = 18  ← 14pt 行高约 18~20，留 2px 内边距刚刚好
     *
     * 视觉上我们把 9 个键画成规整的 3×3（按 keyId 排序），左到右、上到下：
     *   1 2 3
     *   4 5 6
     *   7 8 9 */
    static constexpr int16_t MATRIX_X0 = 8;
    static constexpr int16_t MATRIX_Y0 = 58;
    static constexpr int16_t MATRIX_W = 412;
    static constexpr int16_t MATRIX_H = 64;
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

        /* ---- y=0..26 标题区 ---- */
        lv_obj_t *title = lv_label_create(root_obj);
        lv_label_set_text(title, "KeyMap");
        lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 2);

        /* 标题右侧 profile 文本（[1/4] Numpad 等） */
        profile_label_ = lv_label_create(root_obj);
        lv_obj_set_style_text_color(profile_label_, lv_color_hex(0x4DA3FF), LV_PART_MAIN);
        lv_obj_set_style_text_font(profile_label_, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(profile_label_, LV_ALIGN_TOP_RIGHT, -8, 6);

        /* ---- y=28 分割线 ---- */
        lv_obj_t *line1 = lv_obj_create(root_obj);
        lv_obj_remove_style_all(line1);
        lv_obj_set_size(line1, SCREEN_W_PX - 16, 1);
        lv_obj_set_style_bg_color(line1, lv_color_hex(0x404040), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(line1, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_pos(line1, 8, 28);

        /* ---- y=32..52 当前选中键详情 ---- */
        selected_label_ = lv_label_create(root_obj);
        lv_obj_set_style_text_color(selected_label_, lv_color_hex(0xFFCC00), LV_PART_MAIN);
        lv_obj_set_style_text_font(selected_label_, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(selected_label_, LV_ALIGN_TOP_LEFT, 8, 34);

        /* ---- y=54 分割线 ---- */
        lv_obj_t *line2 = lv_obj_create(root_obj);
        lv_obj_remove_style_all(line2);
        lv_obj_set_size(line2, SCREEN_W_PX - 16, 1);
        lv_obj_set_style_bg_color(line2, lv_color_hex(0x404040), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(line2, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_pos(line2, 8, 54);

        /* ---- y=58..122 3×3 矩阵 ----
         * cellH = 18 px 放下 14pt 行高，label = cell 同位同尺寸 + 居中样式
         * 文本内容由 refresh() 写入 */
        const int16_t w = cellWpx();
        const int16_t h = cellHpx();
        for (uint8_t i = 0; i < 9; ++i)
        {
            /* cell 背景框 */
            lv_obj_t *bg = lv_obj_create(root_obj);
            lv_obj_remove_style_all(bg);
            lv_obj_set_size(bg, w, h);
            lv_obj_set_pos(bg, cellX(i), cellY(i));
            lv_obj_set_style_bg_color(bg, cellNormalBg(), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(bg, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_radius(bg, 4, LV_PART_MAIN);
            cell_bg_[i + 1] = bg;

            /* 文字 label：cell 大小 = label 大小，文本居中样式由样式保证
             * cellH=18 足够放下 14pt 行高（~16~18px）；label 设置与 cell 同位同尺寸，
             * 内部 LV_TEXT_ALIGN_CENTER 让文字水平居中、LVGL 默认基线对齐自动垂直居中。
             * 这样不依赖硬编码 -6/-7 偏移，文本长度变化也能居中。 */
            lv_obj_t *lab = lv_label_create(root_obj);
            lv_obj_set_style_text_color(lab, cellNormalFg(), LV_PART_MAIN);
            lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, LV_PART_MAIN);
            lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_set_size(lab, w, h);
            lv_obj_set_pos(lab, cellX(i), cellY(i));
            /* 文本内容由 refresh() 写入 */
            cell_labels_[i + 1] = lab;
        }

        /* ---- y=120..138 底部 hint 区 ----
         * 按 docs/10-input-mapping-rule.md §5 L 类型模板：
         * "K1 back  KNOB pick  K2 enter  K3..K9 jump"
         * 注：当前 KeyMapPage 的 K2 实际语义是"下一 profile"而非"进入选中项"，
         * 模板里 K2 enter 是 L 类的通用含义，与 KeyMap 的 K2=next profile 略不一致；
         * 这里保留模板以保证跨页一致性，并接受这一处 L 类内的微小例外。
         * （如要严格区分，可在 hint 后追加 "K2 next profile" 等小字片段。） */
        lv_obj_t *hint = lv_label_create(root_obj);
        char hint_buf[80];
        buildHintLabel(kind(), hint_buf, sizeof(hint_buf));
        lv_label_set_text(hint, hint_buf);
        lv_obj_set_style_text_color(hint, lv_color_hex(0x6B7280), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, -8, -4);

        refresh();
    }

    void KeyMapPage::teardownUi()
    {
        profile_label_ = nullptr;
        selected_label_ = nullptr;
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

    /* L 类型 selectItem：idx ∈ [0,8] → keyId = idx + 1。idx 越界返回 false。 */
    bool KeyMapPage::selectItem(uint8_t idx)
    {
        const uint8_t keyId = (uint8_t)(idx + 1U);
        if (keyId < 1 || keyId > 9)
            return false;
        selectedKeyId_ = keyId;
        refresh();
        return true;
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

        /* 当前选中键的详细映射（顶栏黄色文本） */
        if (selected_label_ != nullptr)
        {
            char lbl[8];
            labelForKeyId(selectedKeyId_, lbl, sizeof(lbl));
            char selBuf[32];
            snprintf(selBuf, sizeof(selBuf), "KEY%u = %s", (unsigned)selectedKeyId_, lbl);
            lv_label_set_text(selected_label_, selBuf);
        }

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
                /* 重画文本为 "keyId><label>" */
                char mapTxt[12];
                char lbl[8];
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