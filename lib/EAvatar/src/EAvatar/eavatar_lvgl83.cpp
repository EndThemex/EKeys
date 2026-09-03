#include "eavatar_lvgl83.h"
#include <math.h>

namespace eavatar {

/* ---- 工具：在屏幕坐标系下生成一段 capsule（带旋转/开合）的多边形点列 ----
 *
 * 用 cubic Bezier 近似圆角的思路（k=0.5523... 是 4 次 Bezier
 * 模拟 90° 圆弧的标准控制点系数）。一圈 capsule 由 4 段 cubic 组成。
 * 输出点列存到 out[]（最多 64 个点，4 段 × 16 采样）。*/
static void capsulePoints(lv_point_t *out, int &outN,
                          float cx, float cy,
                          float rx, float ry,
                          float rotDeg, float open)
{
    const float k = 0.5522847498f;
    const float a = rotDeg * (float)EA_PI / 180.0f;
    const float c = cosf(a);
    const float s = sinf(a);
    /* 开合度影响 ry 高度（眨眼时眼睛压扁） */
    const float ryEff = ry * clamp(open, 0.06f, 1.0f);

    /* 把局部坐标 (x, y) 旋转+平移到屏幕坐标 */
    auto tr = [&](float x, float y) -> lv_point_t {
        lv_point_t p;
        p.x = (lv_coord_t)lroundf(cx + x * c - y * s);
        p.y = (lv_coord_t)lroundf(cy + x * s + y * c);
        return p;
    };

    /* 四段 cubic，每段 16 个点近似 → 4×16 = 64 个点 */
    constexpr int SEG = 16;
    int idx = 0;
    /* 第一段：从 (rx, 0) 经 (rx, k*ry) (k*rx, ry) 到 (0, ry) */
    for (int i = 0; i < SEG; ++i) {
        float t = (float)i / (float)SEG;
        float u = 1.0f - t;
        float x = u*u*u * rx + 3*u*u*t * rx + 3*u*t*t * (k * rx) + t*t*t * 0.0f;
        float y = u*u*u * 0  + 3*u*u*t * (k * ryEff) + 3*u*t*t * ryEff + t*t*t * ryEff;
        out[idx++] = tr(x, y);
    }
    /* 第二段：(0, ry) → (-rx, 0)，控制点 (-k*rx, ry) (-rx, k*ry) */
    for (int i = 0; i < SEG; ++i) {
        float t = (float)i / (float)SEG;
        float u = 1.0f - t;
        float x = u*u*u * 0      + 3*u*u*t * (-k * rx) + 3*u*t*t * (-rx) + t*t*t * (-rx);
        float y = u*u*u * ryEff  + 3*u*u*t * ryEff     + 3*u*t*t * (k * ryEff) + t*t*t * 0.0f;
        out[idx++] = tr(x, y);
    }
    /* 第三段：(-rx, 0) → (0, -ry) */
    for (int i = 0; i < SEG; ++i) {
        float t = (float)i / (float)SEG;
        float u = 1.0f - t;
        float x = u*u*u * (-rx)  + 3*u*u*t * (-rx)     + 3*u*t*t * (-k * rx) + t*t*t * 0.0f;
        float y = u*u*u * 0.0f   + 3*u*u*t * (-k * ryEff) + 3*u*t*t * (-ryEff) + t*t*t * (-ryEff);
        out[idx++] = tr(x, y);
    }
    /* 第四段：(0, -ry) → (rx, 0) */
    for (int i = 0; i < SEG; ++i) {
        float t = (float)i / (float)SEG;
        float u = 1.0f - t;
        float x = u*u*u * 0.0f   + 3*u*u*t * (k * rx)  + 3*u*t*t * rx + t*t*t * rx;
        float y = u*u*u * (-ryEff) + 3*u*u*t * (-ryEff) + 3*u*t*t * (-k * ryEff) + t*t*t * 0.0f;
        out[idx++] = tr(x, y);
    }
    outN = idx;
}

/* ---- 工具：blob body 的 64 个采样点转屏幕坐标 ---- */
static void bodyPoints(const AvatarFrame &f,
                       int32_t cx, int32_t cy, int32_t R,
                       lv_point_t *out, int &outN)
{
    outN = PROFILE_SAMPLES;
    for (int i = 0; i < PROFILE_SAMPLES; ++i) {
        float a = (float)i * (float)EA_TAU / (float)PROFILE_SAMPLES;
        float rr = (float)R * f.body_radius[i];
        out[i].x = (lv_coord_t)lroundf((float)cx + cosf(a) * rr);
        out[i].y = (lv_coord_t)lroundf((float)cy + sinf(a) * rr);
    }
}

/* ---- 多边形顶点上限（崩溃根因，勿改大！） ----
 * LVGL 8.3 的软件 draw_polygon 用 line mask 逐边裁剪实现，蒙版槽位
 * _LV_MASK_MAX_NUM = 16（lv_draw_mask.h）。顶点数接近 64 时每条非水平边
 * 都会占一个槽，把列表填满后 lv_draw_mask_apply() 的 while(m->param)
 * 循环会越过数组末尾继续读（真实崩溃：读到野 param → 野 cb=0x1e →
 * PC=0x1e InstrFetchProhibited，Core 1 启动即崩）。
 * 因此每个多边形的顶点必须 ≤ 15，这里取 12 留安全余量。
 * 抽稀用"每次删除偏移量最小的顶点"：圆/蛋等平滑形状均匀简化，
 * 六边形/三角形等角点偏移大、优先保留。凸多边形删点后仍凸。 */
static constexpr int EAVATAR_MAX_POLY_PTS = 12;

static void simplifyPoints(lv_point_t *pts, int &n)
{
    while (n > EAVATAR_MAX_POLY_PTS) {
        int best = 0;
        float bestD = 1e30f;
        for (int i = 0; i < n; ++i) {
            const lv_point_t &a = pts[(i + n - 1) % n];
            const lv_point_t &b = pts[i];
            const lv_point_t &c = pts[(i + 1) % n];
            /* b 到 a-c 连线的垂距 = |cross| / |ac| */
            float ax = (float)a.x, ay = (float)a.y;
            float bx = (float)b.x, by = (float)b.y;
            float cx2 = (float)c.x, cy2 = (float)c.y;
            float dx = cx2 - ax, dy = cy2 - ay;
            float base = sqrtf(dx * dx + dy * dy) + 1e-6f;
            float cross = dx * (by - ay) - dy * (bx - ax);
            float d = fabsf(cross) / base;
            if (d < bestD) {
                bestD = d;
                best = i;
            }
        }
        for (int i = best; i + 1 < n; ++i)
            pts[i] = pts[i + 1];
        --n;
    }
}

/* ============================================================
 *  EAvatar 实现
 * ============================================================ */

lv_obj_t *EAvatar::begin(lv_obj_t *parent, int32_t width, int32_t height)
{
    engine_.begin();

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
    if (self != nullptr) {
        self->engine_.update(lv_tick_get());
        self->invalidate();
    }
}

void EAvatar::drawCb(lv_event_t *e)
{
    auto *self = static_cast<EAvatar *>(lv_event_get_user_data(e));
    if (self == nullptr) return;
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    if (ctx == nullptr) return;
    self->render(ctx);
}

void EAvatar::invalidate()
{
    if (obj_ == nullptr) return;
    /* 只重绘 avatar 本体包围盒，不要 lv_obj_invalidate() 整个 428x142 widget：
     * 30fps 全屏重绘 ≈ 3.6MB/s，SPI@10MHz 带不下，还会每帧连 PNG 背景重刷。
     * 覆盖范围核算：本体半径 ≤ 43(egg+呼吸)，眼睛最远 |x| ≈ 34，
     * Sleep 态 body_y 最大 +0.30 但 scale 仅 0.16 → 合成半径 ≤ 44，
     * 取 ±46/±48 含 AA 余量，accent(±0.675R) 也在其内。 */
    lv_area_t a;
    a.x1 = CX_ - EAVATAR_INV_HALF_W;
    a.x2 = CX_ + EAVATAR_INV_HALF_W;
    a.y1 = CY_ - EAVATAR_INV_HALF_H;
    a.y2 = CY_ + EAVATAR_INV_HALF_H;
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

void EAvatar::event(AvatarEvent e)             { engine_.event(e); invalidate(); }
void EAvatar::setExpression(AvatarExpression e){ engine_.setExpression(e); invalidate(); }
void EAvatar::setGaze(float x, float y)        { engine_.setGaze(x, y); }
void EAvatar::setSpeakingLevel(float v)        { engine_.setSpeakingLevel(v); }
void EAvatar::setState(AvatarState s)          { engine_.setState(s); invalidate(); }

/* ---- 渲染入口 ---- */
void EAvatar::render(lv_draw_ctx_t *ctx)
{
    const AvatarFrame &f = engine_.frame();
    const int32_t cx = CX_ + (int32_t)lroundf(f.body_x + f.shake_x);
    const int32_t cy = CY_ + (int32_t)lroundf(f.body_y);

    /* 1) body */
    drawBody(ctx, f);

    /* 2) 两只眼睛（独立位置/旋转） */
    const int32_t R = BODY_R_PX_;
    for (int i = 0; i < 2; ++i) {
        const float side = (i == 0) ? -1.0f : 1.0f;
        /* 原 EAvatar: ang = (eye_split * side) * π/180
         * 这里 portrait 布局下眼睛重叠，用外推常数 EYE_DX_PX_ 拉开 */
        float ang = (f.eye_split * side) * (float)EA_PI / 180.0f;
        float ex = sinf(ang) * R * 0.52f + side * EYE_DX_PX_;
        float ey = -cosf(ang) * R * 0.02f;
        float depth = clamp(cosf(ang), 0.55f, 1.0f);

        float ex_px = cx + ex;
        float ey_px = cy + ey;

        float rx = f.eyes[i].width * R * 0.5f * depth;
        float ry = f.eyes[i].height * R * 0.5f;
        float open = f.eyes[i].open;
        float localRot = f.eyes[i].tilt + f.gaze_roll;
        lv_opa_t opa = (lv_opa_t)(255 * clamp01(f.eyes[i].alpha));

        if (opa <= LV_OPA_MIN) continue;
        drawEye(ctx, (lv_coord_t)lroundf(ex_px), (lv_coord_t)lroundf(ey_px),
                (lv_coord_t)lroundf(rx), (lv_coord_t)lroundf(ry),
                (int16_t)localRot, lv_color_white(), opa);
    }

    /* 3) accent（高亮小圆点，仅 Thinking / Notify / Burst / Comet 触发） */
    if (f.accent_alpha > 0.01f) {
        float ax = cx + f.accent_x * R;
        float ay = cy + f.accent_y * R;
        lv_point_t pts[64];
        int n = 0;
        capsulePoints(pts, n, ax, ay, 3.5f, 3.5f, 0.0f, 1.0f);
        simplifyPoints(pts, n);
        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = lv_color_white();
        dsc.bg_opa = (lv_opa_t)(255 * f.accent_alpha);
        dsc.radius = 0; /* polygon 已经带圆角了，背景不要叠圆角 */
        ctx->draw_polygon(ctx, &dsc, pts, (uint16_t)n);
    }
}

/* ---- 画 blob body：64 边形 ---- */
void EAvatar::drawBody(lv_draw_ctx_t *ctx, const AvatarFrame &f)
{
    lv_point_t pts[PROFILE_SAMPLES];
    int n = 0;
    bodyPoints(f, CX_ + (int32_t)lroundf(f.body_x + f.shake_x),
               CY_ + (int32_t)lroundf(f.body_y),
               BODY_R_PX_, pts, n);
    simplifyPoints(pts, n);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = lv_color_hex(0x050505);  /* 近黑，与原版一致 */
    dsc.bg_opa = (lv_opa_t)(255 * clamp01(f.alpha));
    dsc.radius = 0; /* 不叠圆角，让多边形边缘成为外轮廓 */
    ctx->draw_polygon(ctx, &dsc, pts, (uint16_t)n);
}

/* ---- 画一只 capsule 眼睛 ---- */
void EAvatar::drawEye(lv_draw_ctx_t *ctx,
                      lv_coord_t cx, lv_coord_t cy,
                      lv_coord_t rx, lv_coord_t ry,
                      int16_t angle_deg,
                      lv_color_t color, lv_opa_t opa)
{
    if (rx <= 0 || ry <= 0) return;
    lv_point_t pts[65]; /* 4*16 + 1 闭合点 */
    int n = 0;
    capsulePoints(pts, n, (float)cx, (float)cy,
                  (float)rx, (float)ry, (float)angle_deg, 1.0f);
    simplifyPoints(pts, n);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = opa;
    dsc.radius = 0;
    ctx->draw_polygon(ctx, &dsc, pts, (uint16_t)n);
}

} // namespace eavatar
