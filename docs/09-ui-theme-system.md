# 阶段 09 — UI 主题系统

> 状态：方案评审通过（2026-09-16 按代码实测修订），待编码
> 关联：[`docs/05-ui-screens.md`](./05-ui-screens.md) / [`docs/PROJECT_LAYOUT.md`](./PROJECT_LAYOUT.md)
>
> 决策记录：
>
> 1. 主题套数 = 不预设，`kThemes[]` 表驱动
> 2. 切屏动画 = 跟主题走（每套主题自带 `anim_fademode/spd/delay`）
> 3. 应用策略 = 后序遍历挂载/卸载**共享 `lv_style_t`**（非逐对象 local style 覆写），SquareLine 屏文件零改动
> 4. 动画实现点 = **双收口**：屏内事件切换改 `ui_helpers.c::_ui_screen_change` 内部（31 处调用不动）+ DisplayTask 发起切换在 [`navigateNow`](../src/tasks/DisplayTask.cpp) 覆写 anim 入参
> 5. 重渲范围 = 13 屏全部参与覆写；采用**懒应用**（切主题只刷当前屏，其余屏进入时在 `navigateNow` 补刷）
> 6. **紧凑单页主题 = 紧凑主页（页名选择器）**：主页替换为手写 `ui_CompactMain`（左时间 / 中间当前页名 / 右状态灯），旋钮切换页名，ENTER **真实跳转**到对应二级页（走 `navigateNow`，tag 正常同步）；ESC / 自动回主页回紧凑主页。13 屏对象保留、正常参与主题覆写
> 7. 单页紧凑主题与多页主题并存：互斥启用，由 `ui_theme_t.is_compact_home` 字段声明
> 8. 布局以 LVGL 实测分辨率 **428×142 横条屏**为准（[`LvglPort.cpp`](../src/display/LvglPort.cpp) kScreenWidth/kScreenHeight），不做竖排菜单
> 9. 运行时状态色（WiFi/电量/READY/RECORDING 等）不走静态遍历，统一改引用主题取色 API

---

## 1. 目标

把当前散落在 16 个 `src/ui/*.c` 中约 200 处硬编码颜色 (`lv_color_hex(0x…)`，实测 198 处)、硬编码切屏动画集中到一张**主题注册表**中。新增 / 修改主题只改一张表，不动任何屏文件、不动 `ui_settings_types.h`、不动 `LV_EVENT_KEY` 路由。

支持两种根本不同的交互形态并存：

- **多页主题**（如默认暗夜玻璃）：13 屏全开放，旋钮=屏间导航，菜单项 = 屏幕标签
- **紧凑主页主题**：主页（`ui_MainScreen`）被替换为紧凑主页 `ui_CompactMain`——左时间/日期/星期，中间显示当前页名（一次一项），右状态灯区；旋钮顺/逆时针切换页名，ENTER 直达该页名对应的**二级页**（真实切屏），二级页内 ESC / 5s 自动回主页回到紧凑主页。一级屏（KeyMapped/Music/Audio/PcStatus/Ha/Setting）在紧凑主题下成为"只过渡不驻留"的屏：用户不经过它们，但对象保留并参与主题覆写

---

## 2. 设计原则

| 原则                    | 含义                                                                                                                                                      |
| ----------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------- |
| SquareLine 屏文件零侵入 | 13 个 `ui_*.c` 一字不改；主题 boot 挂在 [`DisplayTask::run()`](../src/tasks/DisplayTask.cpp) 的 `ui_init()` 之后（项目自有代码，SquareLine 永不重写）     |
| 数据驱动                | 加主题只改 `kThemes[]` 一张表 + `ui_theme_id_t` enum                                                                                                      |
| 动画跟主题走            | 多页主题 `anim_*` 三字段切屏；紧凑主题另有 `page_anim_*` 页名切换动画                                                                                     |
| 共享样式挂载/卸载       | 每主题构造一组共享 `lv_style_t`，遍历屏对象树 `lv_obj_add_style` / `lv_obj_remove_style`；切主题=卸旧挂新，**天然无残留**，且内存远小于逐对象 local style |
| 懒应用                  | 切主题只刷当前屏（`navigateNow` 收口），后台屏打脏标记，进入时补刷                                                                                        |
| 只覆写颜色类样式        | v1 只覆写 bg / text / border / radius；**不覆写 pad**——428×142 上布局逐像素调过（翻页钟卡片、80×80 键位格），统一 pad 会破版                              |
| 状态色运行时化          | WiFi/电量/READY/RECORDING/PC 指示条等运行时 setter 改引用 `ui_theme_color_*` API，不依赖静态遍历                                                          |
| 单页/多页互斥           | 一台设备同一时刻只能激活紧凑或多页之一；切换时创建/销毁 `ui_CompactMain`                                                                                  |
| tag 路由不变            | 紧凑主页复用 `UI_SCREEN_MAIN` tag（g_active_screen_tag 仍为 MAIN），`is_compact_home` 区分两种主页；不加新枚举、不动矩阵键路由                            |

---

## 3. 架构

### 3.1 新增目录

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

### 3.2 公共类型 ([`src/ui/theme/ui_theme.h`](../src/ui/theme/ui_theme.h))

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

// 运行时取色 API（状态色 setter 用，见 §3.7.3）
lv_color_t ui_theme_color_ok(void);
lv_color_t ui_theme_color_warn(void);
lv_color_t ui_theme_color_danger(void);
lv_color_t ui_theme_color_accent(void);
```

> 原方案的 `menu_item_height / menu_visible_rows / menu_focus_idx` 三字段删除：紧凑主页是"一次一项"页名选择器（428×142 放不下竖排多行菜单），不存在行高/可见行数概念。

### 3.3 注册表 ([`src/ui/theme/ui_theme_palette.c`](../src/ui/theme/ui_theme_palette.c))

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

### 3.4 动画接管（双收口）

本项目切屏有**两条真实路径**，主题动画必须两处都接管，缺一不可：

| 路径                                                    | 代码位置                                                            | 触发场景                                                   |
| ------------------------------------------------------- | ------------------------------------------------------------------- | ---------------------------------------------------------- |
| 屏内事件回调 → `_ui_screen_change`（31 处调用）         | [`ui_helpers.c`](../src/ui/ui_helpers.c)                            | 旋钮屏间导航（主页→KeyMapped 等）、二级页 ESC 回一级屏     |
| DisplayTask → `navigateNow()` → 直接 `lv_scr_load_anim` | [`DisplayTask.cpp:404-465`](../src/tasks/DisplayTask.cpp#L404-L465) | 矩阵键进 KEYMAPPED_SECONDARY、Navigate 消息、5s 自动回主页 |

#### 3.4.1 `ui_helpers.c::_ui_screen_change`（唯一需要动 SquareLine 文件的一处）

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

#### 3.4.2 `DisplayTask::navigateNow`

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
    ui_theme_ensure_applied(actual);   // 懒应用：见 §3.7.2
    lv_scr_load_anim(actual, t->anim_fademode, t->anim_spd_ms, t->anim_delay_ms, false);
    last_activity_tick_ = 0;
}
```

> tag 仍写 `UI_SCREEN_MAIN`（紧凑主页与 MainScreen 共用 MAIN tag），`is_compact_home` 区分两种主页形态。`ui_set_active_screen_tag` 的时序、`last_activity_tick_` 清零语义均保持原样。

### 3.5 紧凑主页（[`src/ui/theme/ui_compact_main.c`](../src/ui/theme/ui_compact_main.c)）

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
- 页名切换动画见 §3.6

### 3.6 页名切换动画（[`src/ui/theme/ui_compact_menu.c`](../src/ui/theme/ui_compact_menu.c)）

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

### 3.7 样式应用（[`src/ui/theme/ui_theme_style.c`](../src/ui/theme/ui_theme_style.c) + `ui_theme_apply.c`）

#### 3.7.1 共享样式（关键优化：不是逐对象 local style）

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

> 样式选择规则刻意简单：label 一律 `text_primary`，卡片一律 `bg_card`。需要次级色/强调色的对象（副标题、profile 名、选中态）不靠遍历猜——见 3.7.3，由运行时 setter 显式取主题色。原方案基于 `lv_obj_get_name`（**LVGL 8.3 无此 API，编译不过**）和 `strcmp` 文本匹配的逻辑全部删除。

#### 3.7.2 懒应用

```c
void ui_theme_ensure_applied(lv_obj_t *scr);   // navigateNow 进屏时调用；已应用则短路
void ui_theme_apply_active(void);              // 切主题时刷当前屏
```

- `ui_init()` 之后 boot 时只 apply 启动屏（MainScreen 或 CompactMain）
- 切主题（多页内）只 `apply_active()`，其余屏打脏标记（static bitmap），`navigateNow → ensure_applied` 补刷
- 切主题耗时从"13 屏全刷 ~100ms"降为"单屏 ~10ms"

#### 3.7.3 运行时状态色收口（必要配套改动）

静态遍历管不住、且会反向覆盖静态遍历的运行时 setter，全部改为引用主题取色 API：

| 文件                                                                     | 硬编码点                                 | 改为                                                                                     |
| ------------------------------------------------------------------------ | ---------------------------------------- | ---------------------------------------------------------------------------------------- |
| [`ui_StatusBar.c`](../src/ui/ui_StatusBar.c)                             | WiFi 灰/白/橙、电量红/灰（L172-L218）    | `ui_theme_color_*`：灰=`text_dim`、白=`text_primary`、橙=`state_warn`、红=`state_danger` |
| [`ui_HaScreenSecondary.c`](../src/ui/ui_HaScreenSecondary.c)             | WiFi/TCP/Voice/ModuleA/B 状态值 L81-L125 | 同上（`state_ok`/`state_warn`/`state_danger`）                                           |
| [`ui_PcStatusScreenSecondary.c`](../src/ui/ui_PcStatusScreenSecondary.c) | 网络点、CPU/磁盘指示条 L310-L432         | 同上                                                                                     |
| [`ui_MainScreen.c`](../src/ui/ui_MainScreen.c)                           | profile 名橙色 L194                      | `ui_theme_color_accent()`（映射 accent_yellow）                                          |

此表是**枚举清单**：实施时全局搜索 `lv_color_hex(0x22C55E)|0xF59E0B|0xEF4444|0xFF0000` 逐处替换，替换后这些对象的颜色自动跟主题走。其余约 150 处静态 `lv_color_hex`（创建时写死的底色/描边）由共享样式遍历接管，不改屏文件。

#### 3.7.4 主题切换异步投递与执行

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

### 3.8 入口接入（不碰 SquareLine 文件）

~~原方案在 `ui.c` 的 `ui_init()` 末尾追加~~ — **不成立**：`ui.c` 是 SquareLine 整文件重写的产物（本项目已有手工合并 `ui_StatusBar` include 的先例，每次重导出都要人肉合并，主题 boot 不应再进这个文件）。

改为在 [`DisplayTask::run()`](../src/tasks/DisplayTask.cpp) 内 `ui_init()` 之后追加一段（项目自有代码，SquareLine 永不触碰）：

```cpp
ui_init();
// STAGE_09: 主题系统 boot
ui_theme_boot(Configuration::instance().snapshot().tft_theme);
//   - 读持久化主题 id
//   - 构建该主题共享样式 + apply 启动屏
//   - 若 is_compact_home：ui_compact_main_ensure() + lv_disp_load_scr(紧凑主页)
//   - 多页主题：一切照旧
```

### 3.9 设置屏范围（[`ui_SettingScreenSecondary.c:567-576`](../src/ui/ui_SettingScreenSecondary.c#L567-L576)）

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

**持久化默认值修正**：[`Configuration.cpp:206`](../src/config/Configuration.cpp#L206) 现在 `GetLongValue("display","tft_theme", 0)` 默认 0 = sentinel，而 UI 快照默认 1。改为默认 `UI_THEME_DEFAULT`（1），并在加载后 clamp 到合法 id（非法值回落 UI_THEME_DEFAULT），避免历史 config.ini 里的 0/2/3 裸 id 落进空洞。

> **注意**：紧凑主题下设置屏可从紧凑主页 ENTER 直达（`kCompactPages` 含 SETTING_SECONDARY），**主题切换入口在紧凑主题下依然可用**——原方案"单页主题进不了设置屏、只能工厂重置"的约束取消。设置屏本身是二级屏，紧凑主题下 `navigateNow(UI_SCREEN_SETTING_SECONDARY)` 正常切屏。

### 3.10 旋钮输入路由（[DisplayTask.cpp:262-328](../src/tasks/DisplayTask.cpp#L262-L328)）

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
- 二级页 ESC 回一级屏的 `_ui_screen_change` 被重定向回紧凑主页（§3.4.1），用户无感知
- 5s 自动回主页 `checkAutoReturn → navigateNow(UI_SCREEN_MAIN)` 自动回紧凑主页（§3.4.2 的 MAIN 重定向），无需改 `checkAutoReturn`

---

## 4. 内存评估

| 项                                                  | 大小                                                                  |
| --------------------------------------------------- | --------------------------------------------------------------------- |
| 1 套 `ui_theme_t`                                   | 11 色 × 4B + 2 尺寸 + 3 anim + 3 page_anim + 指针 ≈ 90B × 6 套 < 600B |
| 每主题共享 `lv_style_t` 组（3 个 style × ~4 props） | ~100B × 6 套 ≈ 600B                                                   |
| 对象侧 style 引用（13 屏 ~500 对象 × 平均 1.5 条）  | ~500 × 1.5 × 12B ≈ 9KB（全部挂满后常驻）                              |
| 紧凑主页 `ui_CompactMain`（仅紧凑主题存活）         | ~30 对象 ≈ 3KB                                                        |
| 页名切换 anim runtime                               | 1 × ~120B                                                             |
| **总计新增**                                        | < 15KB（对比原 local style 方案 30~50KB）                             |

当前 LVGL pool = 128KB（[`lv_conf.h:54`](../include/lv_conf.h#L54)）。峰值预计 < 140KB，水位留有安全余量；扩池先例已有（音频屏教训），如逼近再扩到 160KB。

---

## 5. 切换延迟评估

| 动作                                     | 耗时                         |
| ---------------------------------------- | ---------------------------- |
| 多页内切主题（单屏 apply，懒应用）       | ~10ms                        |
| 紧凑 ↔ 多页互斥切换（建/销 CompactMain） | ~50ms                        |
| 紧凑主页页名切换（改文本 + 130ms 淡入）  | 130ms（动画时长，非阻塞）    |
| 进后台屏补刷（navigateNow 顺带）         | ~10ms，用户不可感知          |
| `lv_refr_now` 重绘                       | ~30ms                        |
| **用户感知**                             | 全部路径 < 100ms（动画除外） |

---

## 6. 风险与对策

| 风险                                           | 触发条件                           | 对策                                                                                                                                                                                                      |
| ---------------------------------------------- | ---------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------- | -------- | ------------------------------------------------------ |
| SquareLine 重导出覆盖 `ui_helpers.c`           | 用户在 SquareLine 中修改并重新导出 | 改动只有 §3.4.1 一段（顶部 include + 函数体前 10 行），文件头注释 `// STAGE_09_CUSTOM: 主题动画接管点，重导时合并此段`；`ui.c`/其余屏文件零改动（相比原方案取消 ui.c 追加，重导出面从 2 个文件缩到 1 个） |
| 卡片判定误伤（透明遮罩、状态栏底板等）         | 遍历按 bg_opa 判定                 | `bg_opa != LV_OPA_COVER` 跳过；实施时逐屏目检截图                                                                                                                                                         |
| label 统一 text_primary 后个别对象对比度不足   | 深底上的 dim 文本被刷成白色        | 接受（v1 取舍）；个别对象由运行时 setter 用 `text_dim` 显式覆写（§3.7.3 表扩展）                                                                                                                          |
| 状态色 setter 替换遗漏                         | 实施时搜索不全                     | 全局搜 `0x22C55E                                                                                                                                                                                          | 0xF59E0B | 0xEF4444 | 0xFF0000` 清单化替换（§3.7.3），验收时再搜一遍确认清零 |
| `navigateNow` 时序变化引入回归                 | ensure_applied 插在 load_anim 前   | ensure_applied 只做样式挂载，不碰 tag/timer；测试矩阵覆盖矩阵键进二级页 / 自动回主页路径                                                                                                                  |
| `tft_theme` 历史 config.ini 存有裸 id（0/2/3） | 老设备升级                         | 加载后 clamp：`ui_theme_get` 回落 UI_THEME_DEFAULT；默认值 0 改 1（§3.9）                                                                                                                                 |
| 紧凑主页时间列与 ui_FlipClock 更新路径分叉     | TimeUpdate 双入口                  | applyMessage 单点分发：`is_compact_home ? ui_compact_main_set_time() : ui_FlipClock_update()`                                                                                                             |
| 紧凑主页布局与硬件 LCD 分辨率强耦合            | 换屏                               | 三栏比例按 `LV_HOR_RES/LV_VER_RES` 动态取值；本项目锁定 428×142                                                                                                                                           |
| BebasNeue 无 CJK 字形，页名中文渲染空白        | 页名 label 误用 BebasNeue          | 页名 label 固定 `FontCKJGT28`（教训：BebasNeue 仅 0x20-0x7f）                                                                                                                                             |

---

## 7. 测试矩阵

| 用例                                  | 预期                                                                                    |
| ------------------------------------- | --------------------------------------------------------------------------------------- |
| 启动默认主题（多页）                  | 13 屏按 `kThemes[UI_THEME_DEFAULT]` 渲染；旋钮导航动画为 FADE_IN 200ms（原来恒为 NONE） |
| 设置屏改主题（多页内）                | 当前屏立即变色；切到其它屏时补刷变色；切回旧主题无残留色                                |
| 上位机 `CMD_CONFIG_SET tft_theme=100` | `ui_theme_apply_now(100)`：建 CompactMain 并加载；旋钮切页名；ENTER 直达对应二级页      |
| 紧凑主页旋钮顺/逆时针                 | 页名下/上切换（130ms 淡入），首尾钳位                                                   |
| 紧凑主页 ENTER                        | 直达 `kCompactPages[s_page_idx].target` 二级屏，tag 同步为对应 `UI_SCREEN_*_SECONDARY`  |
| 紧凑主题二级页 ESC                    | `_ui_screen_change` 重定向回紧凑主页（不是一级屏）                                      |
| 紧凑主题 5s 无操作                    | 自动回紧凑主页（非 MainScreen）                                                         |
| 紧凑主题下设置二级页改主题为多页主题  | CompactMain 销毁、回 MainScreen；主题入口可用，无需工厂重置                             |
| 紧凑主题下矩阵键 1~11                 | HID 输出正常；UI 侧无导航副作用（MAIN tag 落"其它屏丢弃"分支）                          |
| 持久化                                | `tft_theme` 写入 `config.ini`，重启后形态与配色恢复；历史非法 id 回落默认               |
| 内存水位                              | `DisplayTask` 池监控无 OOM；峰值 < 140KB                                                |
| 状态色跟主题                          | 改主题后 WiFi/电量/HA 状态值/PC 指示条颜色随之变化                                      |

---

## 8. 实施步骤

| 步骤 | 内容                                                                                                                                           | 风险           |
| ---- | ---------------------------------------------------------------------------------------------------------------------------------------------- | -------------- |
| 1    | 建 `src/ui/theme/` 8 文件 + `kThemes[]`（UI_THEME_DEFAULT + UI_THEME_COMPACT_DARK）+ 稀疏 id 遍历工具                                          | 无             |
| 2    | `ui_theme_style.c` 共享样式构建 + 遍历挂载/卸载 + 懒应用脏标记                                                                                 | 中（卡片判定） |
| 3    | 写 `ui_compact_main.c`（428×142 三栏页名选择器）+ `ui_compact_menu.c` 页名切换                                                                 | 中（手写布局） |
| 4    | 改 `ui_helpers.c::_ui_screen_change`：主题 anim 覆写 + 紧凑主题一级屏重定向（唯一 SquareLine 改动）                                            | 低             |
| 5    | 改 `DisplayTask`：`navigateNow` anim 覆写 + MAIN 重定向 + ensure_applied；`applyMessage` 紧凑主页输入拦截 + ThemeSwitch 消息 + TimeUpdate 分发 | 中（路由改动） |
| 6    | `MainTask`：SettingUpdate 消费处 `tft_theme` 变更 → `ui_theme_request`；`DisplayMessage` 加 `theme_id` 字段                                    | 低             |
| 7    | 改 `ui_SettingScreenSecondary.c`：范围校验 + 名称查表（§3.9）                                                                                  | 低             |
| 8    | 状态色 setter 清单化替换（§3.7.3 表）                                                                                                          | 低（逐处替换） |
| 9    | `Configuration.cpp` tft_theme 默认值/clamp；`platformio.ini` `build_src_filter` 加 `+<theme/>`                                                 | 低             |
| 10   | 编译验证（按规则不主动编，需触发）                                                                                                             | —              |

每步独立可回滚。步骤 2（样式判定规则）与 3（手写紧凑主页）风险最高。

---

## 9. 主题候选总览

| ID  | 名称             | 形态         | 调色板定位                | 动画（切屏/页名）           |
| --- | ---------------- | ------------ | ------------------------- | --------------------------- |
| 1   | 暗夜玻璃（默认） | 多页         | 深蓝黑 + 半透明卡片       | FADE_IN 200ms / —           |
| 2   | 暗夜纯净         | 多页         | 纯黑卡片，无边框          | NONE 0ms / —                |
| 3   | 浅色极简         | 多页         | 米白底 + 灰描边           | MOVE_LEFT 250ms / —         |
| 4   | 高对比无障碍     | 多页         | 黑底 + 亮黄/亮青          | FADE_OUT_TOP 150ms / —      |
| 5   | AMOLED 极黑      | 多页         | `#000000` 全黑 + 1px 分隔 | OVERVIEW 300ms / —          |
| 100 | 紧凑 暗夜        | **紧凑主页** | 左时间/中页名/右状态灯    | FADE_IN 200ms / 淡入 130ms  |
| 101 | 紧凑 浅色        | 紧凑主页     | 米白底 + 蓝选             | FADE_IN 250ms / 淡入 130ms  |
| 102 | 紧凑 高对比      | 紧凑主页     | 黑底 + 亮黄               | OVERVIEW 200ms / 淡入 100ms |

> 紧凑主题仅在 100 段位预留，区间 1~99 给多页主题。
> 字体不在主题差异范围内：当前 LVGL 编译进 Montserrat + 3 个 BebasNeue + 3 个 FontCKJGT（BebasNeue 仅 0x20-0x7f 无符号/CJK 字形），切主题不能换字体。

---

## 10. 变更记录

- 2026-09-16：阶段 09 SPEC 创建。基于用户决策：
  - 主题套数 = 不预设
  - 动画 = 跟主题走
  - 覆盖策略 = 后序遍历
  - 接管点 = 改 `ui_helpers.c` 内部
  - 切换范围 = 默认 13 屏全参与
  - 单页紧凑主题 = 完全隐藏 13 屏 + 竖排菜单 + ENTER 展开详情面板
- 2026-09-16：**按代码实测修订（本轮）**。核对代码后修正以下与事实不符/不可行的设计：
  - **紧凑主题形态重定义**（用户拍板）：428×142 横条屏放不下竖排 5 行菜单（5×36=180px > 142px）；"ENTER 展开详情面板复用 SquareLine 屏对象"与 `g_active_screen_tag` 路由冲突（面板覆盖时 `lv_scr_act()` 仍是主页，键事件到不了面板）。改为**紧凑主页页名选择器**：中间一次一项页名，旋钮切换，ENTER 经 `navigateNow` 直达二级页（真实切屏），ESC/自动回主页重定向回紧凑主页；tag 复用 `UI_SCREEN_MAIN`，不加新枚举
  - **动画接管点补全**：原方案只改 `ui_helpers.c`，漏掉 `DisplayTask::navigateNow`（矩阵键进二级页/Navigate/自动回主页都直接 `lv_scr_load_anim`）——改为双收口
  - **应用策略改为共享 `lv_style_t` 挂载/卸载**：原 local style 逐对象覆写在 ~500 对象上约 30~50KB，128KB pool 承受不住且与"样式池 6KB"的内存评估自相矛盾；共享样式 <15KB 且无残留。补懒应用（切主题只刷当前屏）
  - **删除 `lv_obj_get_name` / strcmp 文本匹配**：LVGL 8.3 无 `lv_obj_get_name` API，编译不过；文本匹配被运行时 setter 反向覆盖。改为状态色统一走 `ui_theme_color_*` API + setter 清单化替换（§3.7.3）
  - **取消 pad 覆写**：428×142 布局逐像素调过（翻页钟、80×80 键位格），统一 pad_all 会破版
  - **`ui_theme_count()` 修正**：稀疏 enum（100 段位）下 `sizeof/sizeof` 会算出 ~101 套，改为遍历统计 + `ui_theme_id_at/index_of` 双向映射
  - **入口从 `ui.c` 移到 `DisplayTask::run()`**：`ui.c` 是 SquareLine 整文件重写产物，"重导出不触碰 ui_init 末尾"不成立；boot 挂项目自有代码永不需合并
  - **紧凑主题下设置屏可达**：页名表含"设置"，原"单页主题只能工厂重置"约束取消
  - 其它：`DisplayMessage` 平铺加 `theme_id` 字段（无 union，`msg.payload.*` 写法错误）；config.ini `tft_theme` 默认 0（sentinel）改 1 并 clamp；紧凑主题切屏动画字段恢复有效；页名中文用 FontCKJGT（BebasNeue 无 CJK）
