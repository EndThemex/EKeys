/**
 * @file ui_FlipClock.c
 * @brief 主页翻页时钟组件实现
 *
 * 结构：6 张等宽大卡片（HH:MM:SS）+ 2 组冒号圆点（HH:MM 与 MM:SS 之间）。
 * 每张卡片 = 卡片容器（圆角裁剪）+ 上下两个半区容器（各含一个整字标签，
 * 通过 y 偏移让数字相对整卡居中，半区只露出各自一半）+ 中缝分割线
 * + 两个隐藏的翻页层（几何与半区一致）。
 *
 * 布局：卡片与冒号槽交替排列（卡:卡:卡卡卡卡），所有块间距统一 FLIP_GAP，
 * 整体水平居中。
 *
 * 数字变化动画（经典翻页钟简化版，两段各 130ms，纯 height/translate 几何动画）：
 *   1. 上翻页层显示「旧数字上半部」，高度折叠（height half_h→0，translate_y
 *      保持下边缘贴住中缝，旧数字随之沉入中缝），折叠完成后露出底图的新数字上半部；
 *   2. 下翻页层显示「新数字下半部」，从中缝向下展开（height 0→half_h）盖过
 *      旧数字下半部，展开完成后隐藏翻页层、底图下半区落定为新数字。
 *
 * 注意：不要改用 transform_zoom 实现折叠——LVGL 8.3 对带子对象的容器做 zoom
 * 会走 layer 变换渲染路径，label 字形缩放后破碎/不显示（参考 K230D 与
 * LILYGO T-Display-S3-Long 翻页时钟项目均因此改用几何动画）。
 *
 * 每秒由 DisplayTask 经 ui_FlipClock_update() 驱动：秒位翻动（冒号常亮）。
 */
#include "ui_FlipClock.h"

#include <string.h>

/* BebasNeueFont48 已在 ui.h 声明（SquareLine 导出字号），六张卡片统一用 48 号 */
LV_FONT_DECLARE(ui_font_BebasNeueFont48);

/* ---------- 布局常量（LVGL 实际分辨率 428x142 横条屏，见 LvglPort.cpp） ---------- */
#define FLIP_CARD_W 56  /* 卡片宽（6 张卡片统一尺寸） */
#define FLIP_CARD_H 92  /* 卡片高（y 4~96，下方整行留给状态/日期栏） */
#define FLIP_COLON_W 14 /* 冒号槽宽 */
#define FLIP_GAP 6      /* 相邻块（卡片/冒号槽）统一间距 */
/* 8 个块（6 卡 + 2 冒号槽）7 个间距，整体水平居中 */
#define FLIP_TOTAL_W (6 * FLIP_CARD_W + 2 * FLIP_COLON_W + 7 * FLIP_GAP)
#define FLIP_X0 ((LV_HOR_RES - FLIP_TOTAL_W) / 2)
#define FLIP_CARD_Y 4    /* 卡片 y */
#define COLON_DOT_SIZE 6 /* 冒号圆点直径 */
#define COLON_DOT_GAP 8  /* 冒号圆点间距 */
#define FLIP_ANIM_MS 130 /* 单段翻页动画时长 */

/* ---------- 配色 ---------- */
#define CARD_BORDER_COLOR 0x3A3A3A
#define CARD_TOP_COLOR 0x2E2E2E
#define CARD_BOT_COLOR 0x222222
#define CARD_DIGIT_COLOR 0xF5F7FA

typedef struct
{
    lv_obj_t *card;     /* 卡片容器（圆角 + 裁剪） */
    lv_obj_t *top;      /* 底图上半区 */
    lv_obj_t *top_lbl;  /* 底图上半区数字 */
    lv_obj_t *bot;      /* 底图下半区 */
    lv_obj_t *bot_lbl;  /* 底图下半区数字 */
    lv_obj_t *flip_top; /* 翻页层：旧数字上半部，向下折叠 */
    lv_obj_t *flip_top_lbl;
    lv_obj_t *flip_bot; /* 翻页层：新数字下半部，向下展开 */
    lv_obj_t *flip_bot_lbl;
    lv_coord_t half_h; /* 半区高度（= 卡片高/2，动画用） */
    char cur;          /* 当前数字 */
    bool animating;    /* 翻页动画进行中 */
} flip_card_t;

static lv_obj_t *s_root = NULL;
static flip_card_t s_cards[6];    /* 0~5: HH:MM:SS */
static lv_obj_t *s_colon_dots[4]; /* 2 组冒号 × 每组 2 圆点 */

/* ---------- 工具 ---------- */

/* 创建半区容器（纯色块）。
 * 关键：lv_obj_create 默认带 LV_OBJ_FLAG_SCROLLABLE，可滚动容器的子对象
 * 不裁剪到容器范围 → 数字整字在上/下半区都完整绘制，翻页时两层叠加
 * 字形"残缺"。这里必须清除 SCROLLABLE，让标签裁剪到半区。 */
static lv_obj_t *half_create(lv_obj_t *card, lv_coord_t y, lv_coord_t w, lv_coord_t h, uint32_t bg)
{
    lv_obj_t *o = lv_obj_create(card);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE); /* 关闭滚动才有子对象裁剪，见函数头注释 */
    lv_obj_set_pos(o, 0, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}

/* 半区内创建整字标签：相对整卡垂直居中，下半区再上移半卡高 */
static lv_obj_t *half_label_create(lv_obj_t *half, const lv_font_t *font, lv_coord_t card_h, bool bottom)
{
    lv_obj_t *lbl = lv_label_create(half);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(CARD_DIGIT_COLOR), 0);
    lv_label_set_text(lbl, "0");
    lv_coord_t y = (card_h - (lv_coord_t)font->line_height) / 2;
    if (bottom)
    {
        y -= card_h / 2;
    }
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, y);
    return lbl;
}

static void label_set_digit(lv_obj_t *lbl, char d)
{
    char buf[2] = {d, '\0'};
    lv_label_set_text(lbl, buf);
}

/* 通过翻页层对象反查所属卡片（避免依赖 user_data 配置） */
static flip_card_t *card_of(const lv_obj_t *flip)
{
    for (int i = 0; i < 6; i++)
    {
        if (s_cards[i].flip_top == flip || s_cards[i].flip_bot == flip)
        {
            return &s_cards[i];
        }
    }
    return NULL;
}

/* 翻页层样式复位（隐藏态下也保持干净起点）。
 * 注意：不用 transform_zoom，LVGL 8.3 对带子对象的容器做 zoom 会走
 * layer 变换渲染路径，label 字形缩放后会出现破碎/不显示（与 K230D
 * LVGL 翻页时钟项目报告的同一 bug）。改用 height + translate_y 纯几何
 * 动画，label 固定不动被收缩容器裁剪，不触发变换路径。 */
static void flip_layer_reset(flip_card_t *c, lv_coord_t half_h)
{
    /* flip_top：满高展开态 */
    lv_obj_set_size(c->flip_top, lv_obj_get_width(c->card), half_h);
    lv_obj_set_pos(c->flip_top, 0, 0);
    lv_obj_set_style_translate_y(c->flip_top, 0, 0);
    /* flip_bot：零高折叠态 */
    lv_obj_set_size(c->flip_bot, lv_obj_get_width(c->card), 0);
    lv_obj_set_pos(c->flip_bot, 0, half_h);
    lv_obj_set_style_translate_y(c->flip_bot, 0, 0);
}

/* ---------- 翻页动画 ---------- */

/* 上半区折叠：height 从 half_h → 0，同时下移使下边缘始终贴住中缝。
 * v 取值 0..1000（线性进度×1000），方便直接用百分比驱动几何。 */
static void fold_top_exec(void *var, int32_t v)
{
    lv_obj_t *o = (lv_obj_t *)var;
    flip_card_t *c = card_of(o);
    if (c == NULL)
        return;
    lv_coord_t half_h = c->half_h;
    lv_coord_t h = (lv_coord_t)(((int32_t)half_h * (1000 - v)) / 1000);
    lv_obj_set_height(o, h);
    /* 下边缘贴中缝： top=0, height=h, 底部在 y=h，需下移 half_h - h */
    lv_obj_set_style_translate_y(o, half_h - h, 0);
}

/* 下半区展开：height 从 0 → half_h，上边缘始终贴住中缝。 */
static void unfold_bot_exec(void *var, int32_t v)
{
    lv_obj_t *o = (lv_obj_t *)var;
    flip_card_t *c = card_of(o);
    if (c == NULL)
        return;
    lv_coord_t half_h = c->half_h;
    lv_coord_t h = (lv_coord_t)(((int32_t)half_h * v) / 1000);
    lv_obj_set_height(o, h);
    /* 上边缘贴中缝：top=half_h, height=h, 顶部已在 y=half_h，无需平移 */
    lv_obj_set_style_translate_y(o, 0, 0);
}

static void unfold_bot_ready(lv_anim_t *a);

/* 第一段完成：隐藏上翻页层，下半区切入第二段 */
static void fold_top_ready(lv_anim_t *a)
{
    flip_card_t *c = card_of(a->var);
    if (c == NULL)
    {
        return;
    }
    lv_obj_add_flag(c->flip_top, LV_OBJ_FLAG_HIDDEN);

    /* 第二段：新数字下半部从中缝展开（底图下半区暂保持旧数字） */
    lv_anim_t an;
    lv_anim_init(&an);
    lv_anim_set_var(&an, c->flip_bot);
    lv_anim_set_exec_cb(&an, unfold_bot_exec);
    lv_anim_set_values(&an, 0, 1000);
    lv_anim_set_time(&an, FLIP_ANIM_MS);
    lv_anim_set_path_cb(&an, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&an, unfold_bot_ready);
    lv_anim_start(&an);
}

/* 第二段完成：底图下半区落定为新数字，翻页层归位 */
static void unfold_bot_ready(lv_anim_t *a)
{
    flip_card_t *c = card_of(a->var);
    if (c == NULL)
    {
        return;
    }
    label_set_digit(c->bot_lbl, c->cur);
    lv_obj_add_flag(c->flip_bot, LV_OBJ_FLAG_HIDDEN);
    flip_layer_reset(c, c->half_h);
    c->animating = false;
}

static void card_set_digit(flip_card_t *c, char d)
{
    if (c->cur == d)
    {
        return;
    }
    char old = c->cur;
    c->cur = d;

    /* 上一次动画未结束又触发：立即落定旧动画再开新动画 */
    if (c->animating)
    {
        lv_anim_del(c->flip_top, fold_top_exec);
        lv_anim_del(c->flip_bot, unfold_bot_exec);
        lv_obj_add_flag(c->flip_top, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(c->flip_bot, LV_OBJ_FLAG_HIDDEN);
        flip_layer_reset(c, c->half_h);
    }
    c->animating = true;

    /* 底图上半区立即显示新数字（被折叠中的翻页层覆盖），
     * 底图下半区保持旧数字，待展开层盖过后再落定 */
    label_set_digit(c->top_lbl, d);
    label_set_digit(c->bot_lbl, old);
    label_set_digit(c->flip_top_lbl, old);
    label_set_digit(c->flip_bot_lbl, d);

    lv_obj_clear_flag(c->flip_top, LV_OBJ_FLAG_HIDDEN);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, c->flip_top);
    lv_anim_set_exec_cb(&a, fold_top_exec);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_time(&a, FLIP_ANIM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_ready_cb(&a, fold_top_ready);
    lv_anim_start(&a);
}

/* ---------- 创建 / 销毁 ---------- */

static void card_create(flip_card_t *c, lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h, const lv_font_t *font)
{
    memset(c, 0, sizeof(*c));

    c->card = lv_obj_create(parent);
    lv_obj_remove_style_all(c->card);
    lv_obj_set_pos(c->card, x, y);
    lv_obj_set_size(c->card, w, h);
    lv_obj_clear_flag(c->card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(c->card, lv_color_hex(CARD_BOT_COLOR), 0);
    lv_obj_set_style_bg_opa(c->card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c->card, h >= 60 ? 10 : 6, 0);
    lv_obj_set_style_clip_corner(c->card, true, 0);
    lv_obj_set_style_border_width(c->card, 1, 0);
    lv_obj_set_style_border_color(c->card, lv_color_hex(CARD_BORDER_COLOR), 0);

    lv_coord_t half_h = h / 2;
    c->top = half_create(c->card, 0, w, half_h, CARD_TOP_COLOR);
    c->bot = half_create(c->card, half_h, w, h - half_h, CARD_BOT_COLOR);
    c->top_lbl = half_label_create(c->top, font, h, false);
    c->bot_lbl = half_label_create(c->bot, font, h, true);

    /* 中缝分割线（翻页层需覆盖它，先建线后建翻页层） */
    lv_obj_t *line = lv_obj_create(c->card);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, 2, half_h - 1);
    lv_obj_set_size(line, w - 4, 2);
    lv_obj_set_style_bg_color(line, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_70, 0);

    c->flip_top = half_create(c->card, 0, w, half_h, CARD_TOP_COLOR);
    c->flip_top_lbl = half_label_create(c->flip_top, font, h, false);
    lv_obj_add_flag(c->flip_top, LV_OBJ_FLAG_HIDDEN);

    c->flip_bot = half_create(c->card, half_h, w, h - half_h, CARD_BOT_COLOR);
    c->flip_bot_lbl = half_label_create(c->flip_bot, font, h, true);
    lv_obj_add_flag(c->flip_bot, LV_OBJ_FLAG_HIDDEN);

    c->half_h = half_h;
    flip_layer_reset(c, half_h);
    c->cur = '0';
}

static lv_obj_t *colon_dot_create(lv_obj_t *parent, lv_coord_t cx, lv_coord_t cy)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_remove_style_all(dot);
    lv_obj_set_pos(dot, cx - COLON_DOT_SIZE / 2,
                   cy - (COLON_DOT_SIZE + COLON_DOT_GAP) / 2 - COLON_DOT_SIZE / 2);
    lv_obj_set_size(dot, COLON_DOT_SIZE, COLON_DOT_SIZE);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(CARD_DIGIT_COLOR), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    return dot;
}

void ui_FlipClock_create(lv_obj_t *parent)
{
    if (parent == NULL)
    {
        return;
    }
    ui_FlipClock_destroy();

    s_root = lv_obj_create(parent);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_pos(s_root, 0, 0);
    lv_obj_set_size(s_root, LV_HOR_RES, LV_VER_RES);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* 8 个块依次排列：卡 卡 : 卡 卡 : 卡 卡，统一间距，水平居中 */
    const lv_coord_t cy = FLIP_CARD_Y + FLIP_CARD_H / 2; /* 冒号垂直中心 */
    int card_idx = 0;
    int dot_idx = 0;
    lv_coord_t x = FLIP_X0;
    for (int slot = 0; slot < 8; slot++)
    {
        if (slot == 2 || slot == 5)
        {
            /* 冒号槽：上下两个圆点 */
            lv_coord_t cx = x + FLIP_COLON_W / 2;
            s_colon_dots[dot_idx++] = colon_dot_create(s_root, cx, cy);
            s_colon_dots[dot_idx++] =
                colon_dot_create(s_root, cx, cy + COLON_DOT_SIZE + COLON_DOT_GAP);
            x += FLIP_COLON_W + FLIP_GAP;
        }
        else
        {
            card_create(&s_cards[card_idx++], s_root, x, FLIP_CARD_Y, FLIP_CARD_W, FLIP_CARD_H,
                        &ui_font_BebasNeueFont48);
            x += FLIP_CARD_W + FLIP_GAP;
        }
    }
}

void ui_FlipClock_destroy(void)
{
    if (s_root != NULL)
    {
        lv_obj_del(s_root); /* 卡片均为子对象，一并删除（动画随对象销毁） */
        s_root = NULL;
    }
    memset(s_cards, 0, sizeof(s_cards));
    memset(s_colon_dots, 0, sizeof(s_colon_dots));
}

/* ---------- 每秒驱动 ---------- */

void ui_FlipClock_update(const char *time_text)
{
    if (s_root == NULL || time_text == NULL)
    {
        return;
    }
    /* 长度与冒号位校验，避免上游格式变化导致越界 */
    if (strlen(time_text) < 8 || time_text[2] != ':' || time_text[5] != ':')
    {
        return;
    }

    card_set_digit(&s_cards[0], time_text[0]);
    card_set_digit(&s_cards[1], time_text[1]);
    card_set_digit(&s_cards[2], time_text[3]);
    card_set_digit(&s_cards[3], time_text[4]);
    card_set_digit(&s_cards[4], time_text[6]);
    card_set_digit(&s_cards[5], time_text[7]);
}
