#pragma once
#include <lvgl.h>
#include "avatar_engine.h"

namespace eavatar
{

    /* EAvatar-LVGL8.3 渲染器
     *
     * 适配 EKeys 项目使用的 LVGL 8.3.11（没有 lv_vector_path_t / lv_layer_t）。
     * 通过自定义 lv_obj + LV_EVENT_DRAW_MAIN 钩子拿 draw_ctx，
     * 再用自研 scanline 填充（偶奇规则 + 边缘分数覆盖 AA，只调 draw_rect）
     * 把 blob body / 椭圆眼睛画到屏幕。不走 draw_polygon：其 line mask 实现
     * 受 _LV_MASK_MAX_NUM=16 限制且满槽有越界读崩溃前科，顶点数被迫 ≤12
     * 导致外形棱角化；scanline 路径无顶点上限且无 mask 开销。
     *
     * 布局参数（针对 EKeys 142×428 portrait）：
     *   - cx = SCREEN_W_PX / 2 = 214（水平居中）
     *   - cy = 78（垂直偏上，留出底部 ~50px 给 hint）
     *   - body_radius = 42 px（直径 84，与 142 高度留出足够空间给眼睛）
     *   - 眼睛在水平方向分开 ~40 px（默认 eye_split_deg 太小，需要外推）
     *
     * 与原 EAvatar v0.2.0 API 等价：begin() / event() / setExpression() /
     * setGaze() / setSpeakingLevel() / setState() / engine() / update()。 */
    class EAvatar
    {
    public:
        EAvatar() = default;

        /* 创建 avatar widget 挂到 parent 上，并启动 LVGL 30Hz 定时器推进动画。
         * 屏幕尺寸由外部传入（用于确定画布范围）。返回创建的 lv_obj。 */
        lv_obj_t *begin(lv_obj_t *parent, int32_t width, int32_t height);

        /* 每帧调用一次：推进引擎 + 触发重绘。一般由 LVGL 定时器自动调用。 */
        void update();

        /* 触发 widget 重绘 */
        void invalidate();

        /* 页面退出时必须调用：删除 30Hz timer 并释放对象引用。
         * lv_timer 不属于 lv_obj 对象树，lv_obj_del(root) 不会删除它；
         * 不 stop 的话 timerCb 会以 33ms 周期访问悬空的 obj_ 导致崩溃。 */
        void stop();

        /* 语义事件：触发状态/表情切换 */
        void event(AvatarEvent e);

        /* 直接设表情 */
        void setExpression(AvatarExpression e);

        /* 设置注视点（x, y 归一化 -1..1） */
        void setGaze(float x, float y);

        /* 设置语音强度 0..1（驱动嘴型/眼睛张开） */
        void setSpeakingLevel(float v);

        /* 强制设置状态 */
        void setState(AvatarState s);

        AvatarEngine &engine() { return engine_; }
        lv_obj_t *object() const { return obj_; }

    private:
        lv_obj_t *obj_ = nullptr;
        lv_timer_t *timer_ = nullptr;
        AvatarEngine engine_;

        /* ---- 布局常量（针对 EKeys 142x428） ---- */
        /* 中心点：水平居中，垂直偏上留 hint 区 */
        static constexpr int32_t CX_ = 214;
        static constexpr int32_t CY_ = 78;
        /* blob body 半径（像素），直径 84，与 142 屏高留 ~30px 给眼睛 */
        static constexpr int32_t BODY_R_PX_ = 42;
        /* 眼睛外推额外偏移（像素）：原 EAvatar 默认 eye_split_deg=15.46°
         * 在 portrait 布局里眼睛会重叠，这里手动外推 eye x。 */
        static constexpr int32_t EYE_DX_PX_ = 16;
        /* 局部重绘包围盒基础半宽/半高（相对 CX_/CY_，像素，不含位移）；
         * invalidate() 会按当前帧 body_x/body_y/shake_x 再动态外扩。
         * 本体半径 ≤ 43(egg+呼吸) × Bounce 拉伸 1.12 + 1px AA ≈ 49。 */
        static constexpr int32_t EAVATAR_INV_HALF_W = 49;
        static constexpr int32_t EAVATAR_INV_HALF_H = 50;

        /* 静态绘制回调 */
        static void timerCb(lv_timer_t *t);
        static void drawCb(lv_event_t *e);

        /* 渲染：从 engine_ 取 AvatarFrame 画到当前 layer（draw_ctx）。
         * 眼睛在 render() 内联绘制（需要 f.eyes[i].open 眨眼开合参数）。 */
        void render(lv_draw_ctx_t *ctx);

        /* 画 blob body：64 个 profile 采样点全保真 scanline 填充 */
        void drawBody(lv_draw_ctx_t *ctx, const AvatarFrame &f);
    };

} // namespace eavatar
