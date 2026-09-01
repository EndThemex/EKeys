# 08 — MenuPage 高亮条"断层"问题排查与修复

> 记录 `MenuPage` 选中高亮条在上下滑动过程中，左右边界出现 1~2 像素抖动（"看起来像断层"）
> 的根因和已采纳的修复方案，避免后续回归。
>
> 日期：2026-09-01

---

## 1. 现象

- 旋钮在 `MenuPage` 的三个菜单项之间切换时，蓝色高亮条跟随 Y 滑动；
- 在动画进行中（即 200 ms 缓动期间），高亮条的左右边缘肉眼可见地左右抖动 1~2 像素，
  视觉上"断"了一下；
- 动画结束后（静态停留某一目标行），边界又是稳定的。

## 2. 根因（按贡献度从大到小）

### 2.1 `lv_obj_set_y()` 逐帧调用会触发父级 layout 重算（主要根因）

```cpp
// ❌ 旧写法：每帧动画调用，都会让 LVGL 重排父级 layout
lv_anim_set_exec_cb(&a, [](void *var, int32_t v) {
    lv_obj_set_y((lv_obj_t *)var, (lv_coord_t)v);
});
```

`lv_obj_set_y()` 在 LVGL 8.3 内部会调用 `_lv_obj_set_pos()`，
最终触发 `lv_obj_update_layout()`，**让父 obj 发起 layout 重排**。
当 200 ms 之内被调用数十次时，每帧都会重算所有 sibling 的位置，
导致 obj 的可视坐标出现亚像素抖动——肉眼看到的就是"左右边界错位"。

### 2.2 `lv_obj_set_style_shadow_*` 让 `coords` 在每帧重新扩张

```cpp
// ❌ 旧写法
lv_obj_set_style_shadow_width(highlight_, 8, LV_PART_MAIN);
lv_obj_set_style_shadow_color(highlight_, highlightColor(), LV_PART_MAIN);
lv_obj_set_style_shadow_opa(highlight_, LV_OPA_30, LV_PART_MAIN);
```

LVGL 的 shadow 不是简单 draw filter——它会让 `obj->coords` 在绘制前向外扩张 8 px，
并把扩张后的尺寸写回 `coords` 区域。逐帧渲染时，扩张大小受 `opa`/`path_cb`/
邻居 obj 影响，并不严格每帧一致，造成视觉上"边界偶尔多出 1 px"。

### 2.3 静态 `lv_obj_set_pos()` 写绝对坐标，没排除父级 padding 影响

```
父 root_ 有 (默认 0) padding； set_pos 不受影响，但和 layout 重算叠加后视觉错位更明显。
```

## 3. 已采纳的修复（不要回退！）

| 修改                                                   | 位置                                 | 为什么                                                                 |
| ------------------------------------------------------ | ------------------------------------ | ---------------------------------------------------------------------- |
| 动画 `exec_cb` 改用 `lv_obj_set_style_translate_y()`   | `MenuPage::animSetY`                 | `translate_y` 只动 style 不过 layout，是 LVGL 官方推荐的"零副作用位移" |
| 高亮条 `style_shadow_*` 全部删除                       | `MenuPage::buildUi` 中的 highlight\_ | 避免 `coords` 逐帧扩张                                                 |
| 高亮条 baseline `y` 写在 `lv_obj_set_pos()` 内         | `MenuPage::buildUi`                  | translate_y 只在 baseline 基础上加偏移                                 |
| 动画的 `lv_anim_set_values(&a, 0, deltaY)`             | `MenuPage::animateToSelected`        | 从"绝对目标 y" 改成 "相对偏移 deltaY"，与 translate_y 语义对齐         |
| `onEnter()` 把 `translate_y` 清零，并 reset baseline y | `MenuPage::onEnter`                  | 跨页面回归时不会残留偏移                                               |

## 4. ⛔ 注意事项（避免再次踩坑）

- **LVGL 8.3 公共头不暴露 `LV_LAYOUT_NONE` / `LV_LAYOUT_OFF` / `lv_layout_t` 这类 NONE 枚举项**
  —— 他们没有同名符号。代码里**不要**写：

  ```cpp
  lv_obj_set_layout(obj, LV_LAYOUT_NONE);   // ❌ 编译失败：'LV_LAYOUT_NONE' was not declared
  lv_obj_set_layout(obj, LV_LAYOUT_OFF);    // ❌ 不存在
  lv_obj_set_layout(obj, (lv_layout_t)0);   // ❌ 'lv_layout_t' 也是 internal
  ```

  **正确做法**：什么都不写。`lv_obj_create()` 默认 layout=0=NONE。

- **不要把 `lv_obj_set_y()` 放在 `lv_anim_t` 的 `exec_cb` 里**；
  需要逐帧位移请用 `lv_obj_set_style_translate_y()` 或 `lv_obj_set_style_translate_x()`。

- **shadow 用法**：需要"光感"时优先用 `lv_style_set_size` + `border` + `outline`，
  而不是 `style_shadow_*`（尤其在需要动画的对象上）。

## 5. 测试检查点

修复后必须验证：

1. 静态时：高亮条左右边界与 row label 对齐；
2. 旋转动画过程中：高亮条左右边界完全无可见抖动；
3. KEY1 退出子页再 KEY2 回来：高亮条准确停在 `selected_` 对应基线 y。

如果发现又出现抖动，第一时间检查：

- 是否有人把 `exec_cb` 改回了 `set_y`；
- 是否给 `highlight_` 加了 `style_shadow_*`；
- 父级 `root_` 是否被改成 `LV_LAYOUT_FLEX` 或 `LV_LAYOUT_GRID`（这种情况才会需要把"layout 是否影响此 obj"再讨论一遍，但目前不是）。

## 6. 相关文件

- [MenuPage.cpp](../../ui/MenuPage.cpp) — 选中条 UI 与动画
- [MenuPage.h](../../ui/MenuPage.h) — 类声明
- [Page.cpp](../../ui/Page.cpp) — `root_` 生命周期
- [PageManager.cpp](../../ui/PageManager.cpp) — 页面栈切换
