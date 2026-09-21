# 2. 架构

> 本文为 [`./README.md`](./README.md) 拆分三块之二：目录结构 + 公共类型 + 注册表 + 动画双收口 + 紧凑主页 + 共享样式与懒应用 + 入口接入 + 设置屏范围 + 旋钮输入路由 + 内存评估。
> 目标、原则、状态色运行时收口、变更记录 → [`1-overview.md`](./1-overview.md)。
> 切换延迟、风险、测试矩阵、实施步骤、主题候选 → [`3-execution.md`](./3-execution.md)。

---

## 2.1 新增目录

```
src/ui/theme/
├── ui_theme.h              // 公共 API + 类型
├── ui_theme_palette.h      // 调色板 POD 类型声明
├── ui_theme_palette.c      // kThemes[] 注册表 + get/count/id_at/index_of
├── ui_theme_style.h        // 共享样式构建 + 挂载/卸载 API
├── ui_theme_style.c        // 每主题一组 lv_style_t 的惰性构建 + 屏对象树遍历挂载
├── ui_theme_apply.h        // 应用 API（apply_screen / ensure_applied / request / apply_now）
├── ui_theme_apply.c        // 懒应用 + 脏标记 + DisplayMessage 投递
├── ui_compact_main.h/.c    // 紧凑主页：左时间 / 中间页名 / 右状态灯（手写，不用 SquareLine）
└── ui_compact_menu.h/.c    // 页名表 + 旋钮切换 + 页名切换动画
```

---

## 2.2 公共类型（[`../../src/ui/theme/ui_theme.h`](../../src/ui/theme/ui_theme.h)）

```c
typedef enum {
    UI_THEME_DEFAULT = 1,           // 多页（默认）
    // 单页主题 enum 从 100 起，预留 1~99 给多页主题
    UI_THEME_COMPACT_DARK = 100,    // 紧凑主页（暗夜）
    // UI_THEME_COMPACT_LIGHT = 101, ...
} ui_theme_id_t;

typedef struct {
    const char *name;             // 设置屏显示用
    bool        is_compact_home;  // true = 紧凑主页主题；false = 多页（默认）

    // 调色板（11 色，覆盖现有 lv_color_hex 用例）
    lv_color_t bg_screen;
    lv_color_t bg_card;
    lv_color_t bg_card_border;
    lv_color_t text_primary;
    lv_color_t text_secondary;
    lv_color_t text_dim;
    lv_color_t state_ok;
    lv_color_t state_warn;
    lv_color_t state_danger;
    lv_color_t accent_cyan;
    lv_color_t accent_yellow;

    // 尺寸（v1 只覆写 radius/border，不覆写 pad）
    uint8_t radius_card;
    uint8_t border_width;

    // 多页主题：切屏动画（navigateNow / _ui_screen_change 两处收口读取）
    lv_scr_load_anim_t anim_fademode;
    uint16_t anim_spd_ms;
    uint16_t anim_delay_ms;

    // 紧凑主题：页名切换动画（中间页名 label 的淡入/位移）
    lv_anim_path_t page_anim_path;
    uint16_t       page_anim_spd_ms;   // 推荐 130~200ms（对齐 FlipClock 130ms 经验值）
    uint16_t       page_anim_delay_ms; // 0
} ui_theme_t;

// 运行时取色 API（状态色 setter 用，见 [1-overview §1.3](./1-overview.md#13-状态色运行时收口必要配套改动)）
lv_color_t ui_theme_color_ok(void);
lv_color_t ui_theme_color_warn(void);
lv_color_t ui_theme_color_danger(void);
lv_color_t ui_theme_color_accent(void);
```

> 原方案的 `menu_item_height / menu_visible_rows / menu_focus_idx` 三字段删除：紧凑主页是"一次一项"页名选择器（428×142 放不下竖排多行菜单），不存在行高/可见行数概念。

---

## 2.3 注册表（[`../../src/ui/theme/ui_theme_palette.c`](../../src/ui/theme/ui_theme_palette.c)）

```c
static const ui_theme_t kThemes[] = {
    [0] = { .name = NULL }, // sentinel

    [UI_THEME_DEFAULT] = {
        .name = "主题 1",
        .is_compact_home = false,
        .bg_screen        = LV_COLOR_MAKE(0x07, 0x0A, 0x0F),
        .bg_card          = LV_COLOR_MAKE(0x0C, 0x0F, 0x14),
        .bg_card_border   = LV_COLOR_MAKE(0x2E, 0x39, 0x47),
        .text_primary     = LV_COLOR_MAKE(0xF5, 0xF8, 0xFF),
        .text_secondary   = LV_COLOR_MAKE(0x8F, 0xA0, 0xB5),
        .text_dim         = LV_COLOR_MAKE(0x9A, 0xA7, 0xB6),
        .state_ok         = LV_COLOR_MAKE(0x22, 0xC5, 0x5E),
        .state_warn       = LV_COLOR_MAKE(0xF5, 0x9E, 0x0B),
        .state_danger     = LV_COLOR_MAKE(0xEF, 0x44, 0x44),
        .accent_cyan      = LV_COLOR_MAKE(0x6A, 0xDA, 0xE1),
        .accent_yellow    = LV_COLOR_MAKE(0xFD, 0xE6, 0x8A),
        .radius_card      = 12,
        .border_width     = 1,
        .anim_fademode    = LV_SCR_LOAD_ANIM_FADE_IN,
        .anim_spd_ms      = 200,
        .anim_delay_ms    = 0,
    },

    [UI_THEME_COMPACT_DARK] = {
        .name = "紧凑 暗夜",
        .is_compact_home = true,
        // 调色板同暗夜玻璃，bg_screen 更深、页名用 accent_cyan
        /* ... 11 色同上 ... */
        .radius_card      = 8,
        .border_width     = 1,
        .anim_fademode    = LV_SCR_LOAD_ANIM_FADE_IN,   // 紧凑主页 ↔ 二级页真实切屏，动画有效
        .anim_spd_ms      = 200,
        .anim_delay_ms    = 0,
        .page_anim_path   = LV_ANIM_PATH_EASE_OUT,
        .page_anim_spd_ms = 130,
        .page_anim_delay_ms = 0,
    },
};

/* 稀疏 enum（1~99 多页 / 100+ 紧凑）下 sizeof(kThemes)/sizeof(kThemes[0])
 * = 最大 id + 1，不能当套数用（100 段位会算出 ~101 套）。
 * 套数与设置屏序号一律遍历统计： */
uint8_t ui_theme_count(void) {
    uint8_t n = 0;
    for (size_t i = 1; i < sizeof(kThemes) / sizeof(kThemes[0]); i++)
        if (kThemes[i].name) n++;
    return n;
}

/* 设置屏序号（1 基）↔ theme id 双向映射 */
ui_theme_id_t ui_theme_id_at(uint8_t index);   // index=1..count，越界返回 UI_THEME_DEFAULT
uint8_t ui_theme_index_of(ui_theme_id_t id);   // 未找到返回 0

const ui_theme_t *ui_theme_get(ui_theme_id_t id) {
    if (id <= 0 || id >= (int)(sizeof(kThemes) / sizeof(kThemes[0]))
            || kThemes[id].name == NULL) {
        return &kThemes[UI_THEME_DEFAULT];
    }
    return &kThemes[id];
}

bool ui_theme_is_compact_home(ui_theme_id_t id) {
    return ui_theme_get(id)->is_compact_home;
}
```

加新主题只改 `kThemes[]` + `ui_theme_id_t` enum，**所有 switch 全部失效**（设计上没有 switch）。

---

## 2.4 动画接管（双收口）

本项目切屏有**两条真实路径**，主题动画必须两处都接管，缺一不可：

| 路径                                                    | 代码位置                                                            | 触发场景                                                   |
| ------------------------------------------------------- | ------------------------------------------------------------------- | ---------------------------------------------------------- |
| 屏内事件回调 → `_ui_screen_change`（31 处调用）         | [`../../src/ui/ui_helpers.c`](../../src/ui/ui_helpers.c)            | 旋钮屏间导航（主页→KeyMapped 等）、二级页 ESC 回一级屏     |
| DisplayTask → `navigateNow()` → 直接 `lv_scr_load_anim` | [`../../src/tasks/DisplayTask.cpp`](../../src/tasks/DisplayTask.cpp) L404-L465 | 矩阵键进 KEYMAPPED_SECONDARY、Navigate 消息、5s 自动回主页 |

### 2.4.1 `ui_helpers.c::_ui_screen_change`（唯一需要动 SquareLine 文件的一处）

```c
// ui_helpers.c (SquareLine 生成；本阶段手动改一次，重导出时按风险表合并)
#include "ui_theme.h"

void _ui_screen_change(lv_obj_t ** target, lv_scr_load_anim_t fademode,
                       int spd, int delay, void (*target_init)(void)) {
    const ui_theme_t *t = ui_theme_active();

    // 紧凑主题：目标是七个一级屏（Main/KeyMapped/Music/Audio/PcStatus/Ha/Setting）
    // 的切屏请求（旋钮导航、二级页 ESC 回一级）一律重定向回紧凑主页。
    // 目标是二级屏的请求（触屏兜底按钮等，本硬件永不触发）保持原样。
    if (t->is_compact_home && ui_theme_target_is_primary(target)) {
        if (ui_compact_main_obj() == NULL) ui_compact_main_create();
        lv_obj_t *home = ui_compact_main_obj();
        lv_scr_load_anim(home, t->anim_fademode, t->anim_spd_ms, t->anim_delay_ms, false);
        return;
    }

    // 多页主题：从主题表覆盖 anim 入参（紧凑主题走 navigateNow，此分支也兼容）
    if (target_init) target_init();
    lv_scr_load_anim(*target, t->anim_fademode, t->anim_spd_ms, t->anim_delay_ms, false);
}
```

`ui_theme_target_is_primary()` 按 `target` 指针与 `&ui_MainScreen` 等 7 个一级屏指针比对（内置表，无屏文件改动）。

### 2.4.2 `DisplayTask::navigateNow`

```c
// DisplayTask.cpp navigateNow() 内，替换硬编码：
//   旧：lv_scr_load_anim(target, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
//   新：
const ui_theme_t *t = ui_theme_active();
lv_obj_t *actual = target;
if (t->is_compact_home && tag == UI_SCREEN_MAIN) {
    // 紧凑主题下"回主页"（Enter 后返回 / Navigate 消息 / 5s 自动回主页）= 回紧凑主页
    actual = ui_compact_main_ensure();
}
if (actual != nullptr && actual != lv_scr_act()) {
    ui_set_active_screen_tag(tag);
    ui_theme_ensure_applied(actual);   // 懒应用：见 §2.7.2
    lv_scr_load_anim(actual, t->anim_fademode, t->anim_spd_ms, t->anim_delay_ms, false);
    last_activity_tick_ = 0;
}
```

> tag 仍写 `UI_SCREEN_MAIN`（紧凑主页与 MainScreen 共用 MAIN tag），`is_compact_home` 区分两种主页形态。`ui_set_active_screen_tag` 的时序、`last_activity_tick_` 清零语义均保持原样。

---

## 2.5 紧凑主页（[`../../src/ui/theme/ui_compact_main.c`](../../src/ui/theme/ui_compact_main.c)）

紧凑主页**手写**（SquareLine 无法表达），一次一项页名选择器，按 428×142 横条屏设计：

```
┌──────────────────────────────────────────────────────────────┐
│ ┌────────────┐  ┌──────────────────────────┐  ┌───────────┐ │
│ │  12:34:56  │  │                          │  │ ● ● ● ● ● │ │
│ │  09月16日  │  │        ▸ 键映射          │  │ ● ● ● ● ● │ │
│ │   星期三   │  │        (BebasNeue36,     │  │  11 键    │ │
│ │            │  │         accent_cyan)     │  │  状态灯   │ │
│ └────────────┘  └──────────────────────────┘  └───────────┘ │
│  左：时间列      中：当前页名（一次一项）        右：状态灯   │
└──────────────────────────────────────────────────────────────┘
```

菜单项 = 6 个二级页入口，与 `ui_screen_tag_t` 一一对应：

```c
// ui_compact_menu.h
typedef struct {
    const char *label;        // "键映射" / "音乐" / "音频" / "PC 状态" / "HA" / "设置"
    ui_screen_tag_t target;   // ENTER 直达的二级页
} ui_compact_page_t;

extern const ui_compact_page_t kCompactPages[];   // {KEYMAPPED_SECONDARY, MUSIC_SECONDARY,
                                                  //  AUDIO_SECONDARY, PC_STATUS_SECONDARY,
                                                  //  HA_SECONDARY, SETTING_SECONDARY}
extern const uint8_t kCompactPageCount;

void ui_compact_main_create(void);      // 惰性创建，返回复用
lv_obj_t *ui_compact_main_ensure(void); // navigateNow / _ui_screen_change 用
void ui_compact_main_destroy(void);     // 切回多页主题时销毁
void ui_compact_page_next(int delta);   // 旋钮：+1/-1 切页名（越界环绕或钳位，取钳位）
void ui_compact_page_enter(void);       // 旋钮单击：DisplayTask 上下文直接 navigateNow(二级 tag)
```

实现要点：

- 根容器 = 全屏 `lv_obj`，三个子容器 `left_col / center_col / right_col` 按绝对坐标布局（30% / 40% / 30%），不嵌 flex，避免滚动标志误触
- 中间页名 = 单个 `lv_label`（`ui_font_BebasNeueFont36` 或中文用 `FontCKJGT28`——**中文页名必须用 CKJGT 字体，BebasNeue 无 CJK 字形**）
- 右状态灯 = 11 个小圆点，数据来自现有 `ui_KeyMappedSecondary_set_key_label` 同源的 DeviceStatus；v1 可先显示连接/工作模式等已有状态，键位状态后续接
- 时间列由 DisplayTask 现有 `TimeUpdate` 消息驱动：`applyMessage` 在紧凑主题下改调 `ui_compact_main_set_time(...)`（一条 if 分支，不动 ui_FlipClock）
- 页名切换动画见 §2.6

---

## 2.6 页名切换动画（[`../../src/ui/theme/ui_compact_menu.c`](../../src/ui/theme/ui_compact_menu.c)）

一次一项，无需整表滚动。切换 = 改文本 + 130ms 淡入（对齐 FlipClock 动画时长经验值）：

```c
void ui_compact_page_next(int delta) {
    const ui_theme_t *t = ui_theme_active();
    int next = s_page_idx + delta;
    if (next < 0 || next >= kCompactPageCount) return;   // 钳位
    s_page_idx = next;

    lv_obj_t *lbl = s_page_label;
    /* 动画中再触发先落定（对齐 ui_FlipClock 的 lv_anim_del 惯例） */
    lv_anim_del(lbl, NULL);

    lv_label_set_text(lbl, kCompactPages[s_page_idx].label);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, lbl);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_set_values(&a, LV_OPA_30, LV_OPA_COVER);
    lv_anim_set_time(&a, t->page_anim_spd_ms);
    lv_anim_set_path_cb(&a, t->page_anim_path);
    lv_anim_start(&a);
}
```

> 原方案的 translate_y + opacity 整表滚动动画删除：那是为竖排多行菜单设计的，428×142 横条屏一次只显示一个页名。

---

## 2.7 样式应用（[`../../src/ui/theme/ui_theme_style.c`](../../src/ui/theme/ui_theme_style.c) + `ui_theme_apply.c`）

### 2.7.1 共享样式（关键优化：不是逐对象 local style）

原方案用 `lv_obj_set_style_*` 逐对象覆写——13 屏约 400~600 个对象 × 4~6 条属性，local style 每条约 12~16B，累计 30~50KB LVGL pool，128KB 池承受不住。改为**每主题一组共享 `lv_style_t`**，遍历时只挂引用（每对象每条引用 ~12B）：

```c
// 每主题惰性构建一次（static，DisplayTask 单线程访问，无锁）
typedef struct {
    lv_style_t screen;   // bg_screen（挂到屏根对象）
    lv_style_t card;     // bg_card + border + radius（挂到"卡片型"容器）
    lv_style_t label;    // text_primary（挂到 lv_label）
} theme_styles_t;

static theme_styles_t s_styles[UI_THEME_CACHE_MAX];  // 按主题 id 缓存，~1KB

static void apply_rec(lv_obj_t *obj, theme_styles_t *st) {
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(obj); i++)
        apply_rec(lv_obj_get_child(obj, i), st);

    lv_obj_class_t *cls = lv_obj_get_class(obj);
    if (obj == lv_scr_act() || lv_obj_get_parent(obj) == NULL) {
        lv_obj_add_style(obj, &st->screen, 0);
        return;
    }
    if (cls == &lv_label_class) {
        lv_obj_add_style(obj, &st->label, 0);
        return;
    }
    // 卡片判定：普通容器/按钮 且当前 bg_opa == COVER（透明按钮、遮罩不误伤）
    if ((cls == &lv_btn_class || cls == &lv_obj_class)
            && lv_obj_get_style_bg_opa(obj, 0) == LV_OPA_COVER) {
        lv_obj_add_style(obj, &st->card, 0);
        return;
    }
}
```

卸载 = 同遍历 `lv_obj_remove_style(obj, &old_style, 0)`。切主题 = 卸旧挂新，无残留；重复 apply 幂等（先查 `lv_obj_find_style` 或跳过已挂）。

> 样式选择规则刻意简单：label 一律 `text_primary`，卡片一律 `bg_card`。需要次级色/强调色的对象（副标题、profile 名、选中态）不靠遍历猜——见 [`1-overview §1.3`](./1-overview.md#13-状态色运行时收口必要配套改动)，由运行时 setter 显式取主题色。原方案基于 `lv_obj_get_name`（**LVGL 8.3 无此 API，编译不过**）和 `strcmp` 文本匹配的逻辑全部删除。

### 2.7.2 懒应用

```c
void ui_theme_ensure_applied(lv_obj_t *scr);   // navigateNow 进屏时调用；已应用则短路
void ui_theme_apply_active(void);              // 切主题时刷当前屏
```

- `ui_init()` 之后 boot 时只 apply 启动屏（MainScreen 或 CompactMain）
- 切主题（多页内）只 `apply_active()`，其余屏打脏标记（static bitmap），`navigateNow → ensure_applied` 补刷
- 切主题耗时从"13 屏全刷 ~100ms"降为"单屏 ~10ms"

### 2.7.3 主题切换异步投递与执行

```c
// 任意任务 → DisplayTask（注意：DisplayMessage 是平铺结构无 union，
// 加字段直接放顶层，不是 msg.payload.theme_id）
void ui_theme_request(ui_theme_id_t id) {
    DisplayMessage msg{};
    msg.type = DisplayMessageType::ThemeSwitch;   // 新增枚举值
    msg.theme_id = (uint8_t)id;
    DisplayTask::postFromIsrOrTask(msg);
}

// DisplayTask 上下文执行
void ui_theme_apply_now(ui_theme_id_t id) {
    bool old_compact = ui_theme_is_compact_home(ui_theme_active_id());
    ui_theme_set_active(id);                       // 置脏标记（全屏待刷）
    bool new_compact = ui_theme_is_compact_home(id);

    if (old_compact != new_compact) {
        if (new_compact) {
            ui_compact_main_ensure();
            ui_set_active_screen_tag(UI_SCREEN_MAIN);
            lv_scr_load_anim(ui_compact_main_obj(), /*anim 取主题表*/...);
        } else {
            ui_compact_main_destroy();
            ui_set_active_screen_tag(UI_SCREEN_MAIN);
            lv_scr_load_anim(ui_MainScreen, ...);
        }
    } else {
        ui_theme_apply_active();                   // 同形态：只刷当前屏
    }
}
```

---

## 2.8 入口接入（不碰 SquareLine 文件）

~~原方案在 `ui.c` 的 `ui_init()` 末尾追加~~ — **不成立**：`ui.c` 是 SquareLine 整文件重写的产物（本项目已有手工合并 `ui_StatusBar` include 的先例，每次重导出都要人肉合并，主题 boot 不应再进这个文件）。

改为在 [`../../src/tasks/DisplayTask.cpp`](../../src/tasks/DisplayTask.cpp) 内 `ui_init()` 之后追加一段（项目自有代码，SquareLine 永不触碰）：

```cpp
ui_init();
// STAGE_09: 主题系统 boot
ui_theme_boot(Configuration::instance().snapshot().tft_theme);
//   - 读持久化主题 id
//   - 构建该主题共享样式 + apply 启动屏
//   - 若 is_compact_home：ui_compact_main_ensure() + lv_disp_load_scr(紧凑主页)
//   - 多页主题：一切照旧
```

---

## 2.9 设置屏范围（[`../../src/ui/ui_SettingScreenSecondary.c`](../../src/ui/ui_SettingScreenSecondary.c) L567-L576）

```c
// 原：if (next < 1 || next > 3 || next == s_setting_edit.tft_theme)
// 新（tft_theme 存的是 theme id；范围校验用主题表实际条目）：
uint8_t cur_idx = ui_theme_index_of((ui_theme_id_t)s_setting_edit.tft_theme);
uint8_t next_idx = (uint8_t)((int)cur_idx + step);
if (next_idx < 1 || next_idx > ui_theme_count())
    return 0;
s_setting_edit.tft_theme = (int32_t)ui_theme_id_at(next_idx);
```

主题名展示从 `setting_secondary_theme_text()` switch 改查表：

```c
case SETTING_ITEM_TFT_THEME:
    snprintf(buffer, buffer_size, "%s",
             ui_theme_get((ui_theme_id_t)s_setting_edit.tft_theme)->name);
    break;
```

**持久化默认值修正**：[`../../src/config/Configuration.cpp`](../../src/config/Configuration.cpp) L206 现在 `GetLongValue("display","tft_theme", 0)` 默认 0 = sentinel，而 UI 快照默认 1。改为默认 `UI_THEME_DEFAULT`（1），并在加载后 clamp 到合法 id（非法值回落 UI_THEME_DEFAULT），避免历史 config.ini 里的 0/2/3 裸 id 落进空洞。

> **注意**：紧凑主题下设置屏可从紧凑主页 ENTER 直达（`kCompactPages` 含 SETTING_SECONDARY），**主题切换入口在紧凑主题下依然可用**——原方案"单页主题进不了设置屏、只能工厂重置"的约束取消。设置屏本身是二级屏，紧凑主题下 `navigateNow(UI_SCREEN_SETTING_SECONDARY)` 正常切屏。

---

## 2.10 旋钮输入路由（[`../../src/tasks/DisplayTask.cpp`](../../src/tasks/DisplayTask.cpp) L262-L328）

紧凑主题的输入拦截**只发生在主页**（`g_active_screen_tag() == UI_SCREEN_MAIN` 且 `is_compact_home`），二级页内旋钮行为（SettingSecondary 调值等）与多页主题完全一致：

```cpp
case DisplayMessageType::ActionInput:
    bumpActivity();

    // 矩阵键（101~111）分支保持原样：紧凑主题下 tag==MAIN 时落入
    // "其它屏：丢弃" 分支（HID 专用语义不变，MainTask 侧 HID 派发不感知主题）

    // 紧凑主页：旋钮改页名选择（新增，放在矩阵键分支之后、原透传之前）
    if (ui_theme_active()->is_compact_home
            && ui_get_active_screen_tag() == UI_SCREEN_MAIN)
    {
        switch (msg.action) {
        case LV_KEY_LEFT:  ui_compact_page_next(-1); break;   // 逆时针 = 上一页名
        case LV_KEY_RIGHT: ui_compact_page_next(+1); break;   // 顺时针 = 下一页名
        case LV_KEY_ENTER: ui_compact_page_enter(); break;    // 直达二级页（navigateNow）
        case LV_KEY_ESC:   break;                             // 主页双击无操作
        default: break;                                       // UP/DOWN 无物理源
        }
        break;
    }

    // 原旋钮 LV_KEY_* 透传 lv_event_send 逻辑不变
```

与原方案的差异：

- 拦截条件从"单页主题下全局拦截所有输入"收窄为"紧凑主页这一屏"，避免破坏二级页内 SettingSecondary 调值 / KEYMAPPED_SECONDARY 焦点跳转等既有行为
- ENTER 直达二级页走 `navigateNow`（DisplayTask 内部直接调），不经 `_ui_screen_change`，tag 正常同步
- 二级页 ESC 回一级屏的 `_ui_screen_change` 被重定向回紧凑主页（§2.4.1），用户无感知
- 5s 自动回主页 `checkAutoReturn → navigateNow(UI_SCREEN_MAIN)` 自动回紧凑主页（§2.4.2 的 MAIN 重定向），无需改 `checkAutoReturn`

---

## 2.11 内存评估

| 项                                                  | 大小                                                                  |
| --------------------------------------------------- | --------------------------------------------------------------------- |
| 1 套 `ui_theme_t`                                   | 11 色 × 4B + 2 尺寸 + 3 anim + 3 page_anim + 指针 ≈ 90B × 6 套 < 600B |
| 每主题共享 `lv_style_t` 组（3 个 style × ~4 props） | ~100B × 6 套 ≈ 600B                                                   |
| 对象侧 style 引用（13 屏 ~500 对象 × 平均 1.5 条）  | ~500 × 1.5 × 12B ≈ 9KB（全部挂满后常驻）                              |
| 紧凑主页 `ui_CompactMain`（仅紧凑主题存活）         | ~30 对象 ≈ 3KB                                                        |
| 页名切换 anim runtime                               | 1 × ~120B                                                             |
| **总计新增**                                        | < 15KB（对比原 local style 方案 30~50KB）                             |

当前 LVGL pool = 128KB（[`../../include/lv_conf.h`](../../include/lv_conf.h) L54）。峰值预计 < 140KB，水位留有安全余量；扩池先例已有（音频屏教训），如逼近再扩到 160KB。
