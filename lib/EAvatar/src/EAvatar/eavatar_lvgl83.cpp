#include "eavatar_lvgl83.h"
#include <math.h>

namespace eavatar
{

    /* ============================================================
     *  渲染路径：自研 scanline 填充（偶奇规则 + 边缘 1px 抗锯齿）
     *
     *  为什么不走 LVGL 8.3 的 draw_polygon：
     *  1) 它用 line mask 逐边裁剪，槽位上限 _LV_MASK_MAX_NUM=16
     *     （lv_draw_mask.h 里是无 #ifndef 保护的死 #define，
     *     lv_conf.h / build_flags 都改不了）；
     *  2) 槽位填满后 lv_draw_mask_apply() 的 while(m->param) 会越过数组
     *     末尾继续读 → 野指针崩溃（本项目真实踩过：PC=0x1e
     *     InstrFetchProhibited，Core 1 启动即崩）；
     *  3) 于是旧代码被迫把 64 点轮廓贪心抽稀到 12 边形，blob 肉眼可见地
     *     "棱角化"，且每帧白烧 ~6000 次 sqrtf；
     *  4) sw polygon 本身没有边缘抗锯齿。
     *
     *  scanline 填充只调 draw_rect（1px 高横线，radius=0 不带任何 mask），
     *  顶点数从此没有硬上限：body 直接用全部 64 个 profile 采样点，
     *  轮廓 1:1 保真（Hexagon/Egg 的棱角不再被抽稀磨掉），边缘由分数
     *  覆盖率 AA 平滑。每帧 0 次 sqrtf、0 次堆分配，CPU 开销低于旧路径。
     * ============================================================ */

    struct FPt
    {
        float x, y;
    };

    /* ---- 三角函数 LUT：一圈 64 等分 ----
     * body 用全部 64 点；眼睛隔点取 32 点；accent 同样复用。
     * begin() 时算一次，之后每帧 0 次 sinf/cosf
     * （ESP32-S3 有 FPU 但 libm 的 sinf/cosf 仍是软实现，每次几十~上百周期）。 */
    static float s_cosLut[PROFILE_SAMPLES];
    static float s_sinLut[PROFILE_SAMPLES];
    static bool s_lutReady = false;

    static void ensureTrigLut()
    {
        if (s_lutReady)
            return;
        for (int i = 0; i < PROFILE_SAMPLES; ++i)
        {
            const float a = (float)i * (EA_TAU / (float)PROFILE_SAMPLES);
            s_cosLut[i] = cosf(a);
            s_sinLut[i] = sinf(a);
        }
        s_lutReady = true;
    }

    /* ---- 偶奇规则 scanline 填充，边缘像素按分数覆盖率半透明合成 ----
     * p: 屏幕坐标浮点顶点（顺/逆时针均可）；n: 顶点数。
     * 每条扫描线在像素中心 (y+0.5) 处采样求交，交点排序后两两配对成填充段；
     * 段两端不足 1px 的部分单独以 opa*coverage 画 1x1，实现水平方向 AA。 */
    static void fillPoly(lv_draw_ctx_t *ctx, const FPt *p, int n,
                         lv_color_t color, lv_opa_t opa)
    {
        if (n < 3 || opa <= LV_OPA_TRANSP)
            return;

        float yMin = p[0].y, yMax = p[0].y;
        for (int i = 1; i < n; ++i)
        {
            if (p[i].y < yMin)
                yMin = p[i].y;
            if (p[i].y > yMax)
                yMax = p[i].y;
        }
        int y0 = (int)ceilf(yMin);
        int y1 = (int)floorf(yMax);
        if (y1 < y0)
            y1 = y0; /* 高度不足 1px 的退化形状（闭眼缝）：至少扫一条线 */

        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = color;
        dsc.radius = 0;

        float xs[PROFILE_SAMPLES]; /* 一条扫描线最多 n 个交点 */
        for (int y = y0; y <= y1; ++y)
        {
            const float yc = (float)y + 0.5f;
            int m = 0;
            for (int i = 0; i < n; ++i)
            {
                const int j = (i + 1) % n;
                const float ay = p[i].y;
                const float by = p[j].y;
                /* 半开区间判定：顶点不重复计数；水平边自动排除，且保证 by != ay */
                if ((ay <= yc) != (by <= yc))
                {
                    xs[m++] = p[i].x + (yc - ay) * (p[j].x - p[i].x) / (by - ay);
                }
            }
            if (m < 2)
                continue;

            /* 交点从左到右排序（凸/星形 m=2，插排足够） */
            for (int i = 1; i < m; ++i)
            {
                const float v = xs[i];
                int j = i - 1;
                while (j >= 0 && xs[j] > v)
                {
                    xs[j + 1] = xs[j];
                    --j;
                }
                xs[j + 1] = v;
            }

            /* 偶奇规则：第 0-1、2-3… 对之间是内部 */
            for (int k = 0; k + 1 < m; k += 2)
            {
                const float xa = xs[k];
                const float xb = xs[k + 1];
                if (xb - xa < 0.01f)
                    continue;

                const int ia = (int)floorf(xa);
                const int ib = (int)floorf(xb);
                lv_area_t line;
                line.y1 = (lv_coord_t)y;
                line.y2 = (lv_coord_t)y;

                if (ia == ib)
                {
                    /* 跨度不足 1px（尖锐顶点）：整条按覆盖率画一个像素 */
                    const float cov = xb - xa;
                    if (opa * cov >= 1.0f)
                    {
                        line.x1 = (lv_coord_t)ia;
                        line.x2 = (lv_coord_t)ia;
                        dsc.bg_opa = (lv_opa_t)(opa * cov);
                        ctx->draw_rect(ctx, &dsc, &line);
                    }
                    continue;
                }

                const float covA = (float)(ia + 1) - xa; /* 左边缘覆盖率 */
                const float covB = xb - (float)ib;       /* 右边缘覆盖率 */
                const int sx1 = (covA >= 0.996f) ? ia : ia + 1;
                const int sx2 = (covB >= 0.996f) ? ib : ib - 1;

                /* 中间实心段 */
                if (sx1 <= sx2)
                {
                    line.x1 = (lv_coord_t)sx1;
                    line.x2 = (lv_coord_t)sx2;
                    dsc.bg_opa = opa;
                    ctx->draw_rect(ctx, &dsc, &line);
                }
                /* 左/右边缘 AA 像素 */
                if (covA < 0.996f && opa * covA >= 1.0f)
                {
                    line.x1 = (lv_coord_t)ia;
                    line.x2 = (lv_coord_t)ia;
                    dsc.bg_opa = (lv_opa_t)(opa * covA);
                    ctx->draw_rect(ctx, &dsc, &line);
                }
                if (covB < 0.996f && opa * covB >= 1.0f)
                {
                    line.x1 = (lv_coord_t)ib;
                    line.x2 = (lv_coord_t)ib;
                    dsc.bg_opa = (lv_opa_t)(opa * covB);
                    ctx->draw_rect(ctx, &dsc, &line);
                }
            }
        }
    }

    /* ---- blob body：64 个 profile 采样点 → 屏幕坐标浮点点列 ----
     * 在这里应用 body_scale / body_squash / body_rotation
     * （旧路径漏乘 body_scale，Sleep 缩小、Burst/Comet 脉冲、呼吸 ±1% 全都看不见）。
     * body_rotation（度）换算成 LUT 采样相位偏移 + 线性插值：
     * 圆形 profile 旋转不可见，Egg/Hexagon/Play 等非圆 profile 才有视觉旋转。 */
    static void bodyPointsF(const AvatarFrame &f,
                            float cx, float cy, float R, FPt *out)
    {
        const float shift = f.body_rotation * ((float)PROFILE_SAMPLES / 360.0f);
        const float sx = f.body_scale;
        const float sy = f.body_scale * f.body_squash;
        for (int i = 0; i < PROFILE_SAMPLES; ++i)
        {
            float idx = (float)i - shift;
            idx -= floorf(idx * (1.0f / (float)PROFILE_SAMPLES)) * (float)PROFILE_SAMPLES;
            const int i0 = (int)idx;
            const float fr = idx - (float)i0;
            const float r0 = f.body_radius[i0];
            const float r1 = f.body_radius[(i0 + 1) & (PROFILE_SAMPLES - 1)];
            const float rr = R * (r0 + (r1 - r0) * fr);
            out[i].x = cx + s_cosLut[i] * rr * sx;
            out[i].y = cy + s_sinLut[i] * rr * sy;
        }
    }

    /* ---- 眼睛：参数方程椭圆 32 点（隔点取 LUT）+ 旋转 + 眨眼压扁 ----
     * 旧实现用 4 段 cubic Bezier 逼近的 capsule，控制点系数 k=0.5523
     * 本来就是椭圆的标准逼近，直接走椭圆参数方程等价且更省。 */
    static void ellipsePointsF(FPt *out, int &outN,
                               float cx, float cy, float rx, float ry,
                               float rotDeg, float open)
    {
        const float a = rotDeg * (EA_PI / 180.0f);
        const float c = cosf(a);
        const float s = sinf(a);
        const float ryEff = ry * clamp(open, 0.06f, 1.0f);
        int idx = 0;
        for (int i = 0; i < PROFILE_SAMPLES; i += 2)
        {
            const float x = s_cosLut[i] * rx;
            const float y = s_sinLut[i] * ryEff;
            out[idx].x = cx + x * c - y * s;
            out[idx].y = cy + x * s + y * c;
            ++idx;
        }
        outN = idx;
    }

    /* ============================================================
     *  EAvatar 实现
     * ============================================================ */

    lv_obj_t *EAvatar::begin(lv_obj_t *parent, int32_t width, int32_t height)
    {
        engine_.begin();
        ensureTrigLut();

        obj_ = lv_obj_create(parent);
        lv_obj_remove_style_all(obj_);
        lv_obj_set_size(obj_, width, height);
        /* 让 avatar widget 完全透明，背景透出 PageManager 的 PNG 背景图 */
        lv_obj_set_style_bg_opa(obj_, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(obj_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(obj_, 0, LV_PART_MAIN);
        /* 自己画整个 body+eyes，不要走默认 LVGL 背景流程 */
        lv_obj_add_event_cb(obj_, drawCb, LV_EVENT_DRAW_MAIN, this);

        /* 30 FPS 推进引擎（≈33ms），与原 EAvatar v0.2.0 一致 */
        timer_ = lv_timer_create(timerCb, 33, this);

        /* 上电默认 WakeUp 状态 */
        event(AvatarEvent::WakeUp);
        return obj_;
    }

    void EAvatar::timerCb(lv_timer_t *t)
    {
        /* LVGL 8.3.11 没有 lv_timer_get_user_data()；
         * user_data 是 lv_timer_t 的公开 struct 字段，直接读 t->user_data 即可。 */
        auto *self = static_cast<EAvatar *>(t->user_data);
        if (self != nullptr)
        {
            self->engine_.update(lv_tick_get());
            self->invalidate();
        }
    }

    void EAvatar::drawCb(lv_event_t *e)
    {
        auto *self = static_cast<EAvatar *>(lv_event_get_user_data(e));
        if (self == nullptr)
            return;
        lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
        if (ctx == nullptr)
            return;
        self->render(ctx);
    }

    void EAvatar::invalidate()
    {
        if (obj_ == nullptr)
            return;
        /* 只重绘 avatar 本体包围盒，不要 lv_obj_invalidate() 整个 428x142 widget：
         * 30fps 全屏重绘 ≈ 3.6MB/s，SPI@10MHz 带不下，还会每帧连 PNG 背景重刷。
         * 基础覆盖：本体半径 ≤ 43(egg+呼吸) × Bounce 拉伸 1.12 + 1px AA
         * ≈ 49，眼睛/accent 都在其内；再按当前帧的 body_x/body_y/shake_x
         * 动态外扩（Bounce 跳起 -14px、Sway/ShakeHead 横移 ±3px），
         * 否则动作动画会被固定包围盒裁掉留下残影。
         * 最差盒 ≈ 98×128px ≈ 20ms/帧 @10MHz，仍在 33ms 帧预算内。 */
        const AvatarFrame &f = engine_.frame();
        const int32_t hw = EAVATAR_INV_HALF_W +
                           (int32_t)ceilf(fabsf(f.body_x) + fabsf(f.shake_x));
        const int32_t hh = EAVATAR_INV_HALF_H + (int32_t)ceilf(fabsf(f.body_y));
        lv_area_t a;
        a.x1 = CX_ - hw;
        a.x2 = CX_ + hw;
        a.y1 = CY_ - hh;
        a.y2 = CY_ + hh;
        lv_obj_invalidate_area(obj_, &a);
    }

    void EAvatar::update()
    {
        engine_.update(lv_tick_get());
        invalidate();
    }

    void EAvatar::stop()
    {
        /* lv_timer 不随 lv_obj 树一起销毁，必须显式删除：
         * 否则页面 exit() → lv_obj_del(root_) 之后，本 timer 仍以 33ms 周期
         * 触发 timerCb → invalidate() 访问悬空的 obj_（页面 push 后立即
         * LoadProhibited 崩溃的根因）。obj_ 同时置空，invalidate() 内部的
         * nullptr 判断成为双保险。再次 begin() 会重建 obj_ 与 timer_。 */
        if (timer_ != nullptr)
        {
            lv_timer_del(timer_);
            timer_ = nullptr;
        }
        obj_ = nullptr;
    }

    void EAvatar::event(AvatarEvent e)
    {
        engine_.event(e);
        invalidate();
    }
    void EAvatar::setExpression(AvatarExpression e)
    {
        engine_.setExpression(e);
        invalidate();
    }
    void EAvatar::setGaze(float x, float y) { engine_.setGaze(x, y); }
    void EAvatar::setSpeakingLevel(float v) { engine_.setSpeakingLevel(v); }
    void EAvatar::setState(AvatarState s)
    {
        engine_.setState(s);
        invalidate();
    }

    /* ---- 渲染入口 ---- */
    void EAvatar::render(lv_draw_ctx_t *ctx)
    {
        const AvatarFrame &f = engine_.frame();
        const float cx = (float)CX_ + f.body_x + f.shake_x;
        const float cy = (float)CY_ + f.body_y;

        /* 1) body */
        drawBody(ctx, f);

        /* 2) 两只眼睛（独立位置/旋转）
         * 眼睛几何（偏移 + 半径）跟随 body_scale/body_squash：
         * Burst/Comet 内爆时眼睛一起缩，Bounce 压扁时眼睛一起压，才像长在脸上。 */
        const float R = (float)BODY_R_PX_;
        const float bs = f.body_scale;
        const float sq = f.body_squash;
        /* gaze_yaw/pitch（度）→ 眼球平移（px）：以 Neutral 姿态 (28.49°, 28.62°)
         * 为零点（sin 值 0.4772/0.4787），差值驱动眼睛在眼眶范围内移动，
         * 钳制 ±5px 防止极端表情把眼睛拉出身体。
         * 没有这一步 gaze_yaw/pitch 只进不出：LookAround、旋钮 setGaze
         * 和各表情里的视线参数全都不可见。 */
        const float gazePx = R * 0.30f;
        const float gx = clamp((sinf(f.gaze_yaw * (EA_PI / 180.0f)) - 0.4772f) * gazePx, -5.0f, 5.0f) * bs;
        const float gy = clamp((0.4787f - sinf(f.gaze_pitch * (EA_PI / 180.0f))) * gazePx, -5.0f, 5.0f) * bs * sq;
        for (int i = 0; i < 2; ++i)
        {
            const float side = (i == 0) ? -1.0f : 1.0f;
            /* 原 EAvatar: ang = (eye_split * side) * π/180
             * 这里 portrait 布局下眼睛重叠，用外推常数 EYE_DX_PX_ 拉开 */
            const float ang = (f.eye_split * side) * (EA_PI / 180.0f);
            const float ex = (sinf(ang) * R * 0.52f + side * (float)EYE_DX_PX_) * bs + gx;
            const float ey = -cosf(ang) * R * 0.02f * bs * sq + gy;
            const float depth = clamp(cosf(ang), 0.55f, 1.0f);

            const float rx = f.eyes[i].width * R * 0.5f * depth * bs;
            const float ry = f.eyes[i].height * R * 0.5f * bs * sq;
            const float open = f.eyes[i].open;
            const float localRot = f.eyes[i].tilt + f.gaze_roll;
            const lv_opa_t opa = (lv_opa_t)(255.0f * clamp01(f.eyes[i].alpha) + 0.5f);

            if (opa <= LV_OPA_TRANSP || rx <= 0.0f || ry <= 0.0f)
                continue;

            FPt pts[PROFILE_SAMPLES / 2];
            int n = 0;
            ellipsePointsF(pts, n, cx + ex, cy + ey, rx, ry, localRot, open);
            fillPoly(ctx, pts, n, lv_color_white(), opa);
        }

        /* 3) accent（高亮小圆点，Thinking / Notify / Burst / Comet / Orbit 触发） */
        if (f.accent_alpha > 0.01f)
        {
            FPt pts[PROFILE_SAMPLES / 2];
            int n = 0;
            ellipsePointsF(pts, n, cx + f.accent_x * R * bs, cy + f.accent_y * R * bs,
                           3.5f, 3.5f, 0.0f, 1.0f);
            fillPoly(ctx, pts, n, lv_color_white(),
                     (lv_opa_t)(255.0f * f.accent_alpha + 0.5f));
        }
    }

    /* ---- 画 blob body：64 边形（轮廓全保真） ---- */
    void EAvatar::drawBody(lv_draw_ctx_t *ctx, const AvatarFrame &f)
    {
        FPt pts[PROFILE_SAMPLES];
        bodyPointsF(f,
                    (float)CX_ + f.body_x + f.shake_x,
                    (float)CY_ + f.body_y,
                    (float)BODY_R_PX_, pts);
        fillPoly(ctx, pts, PROFILE_SAMPLES,
                 lv_color_hex(0x050505), /* 近黑，与原版一致 */
                 (lv_opa_t)(255.0f * clamp01(f.alpha) + 0.5f));
    }

} // namespace eavatar
