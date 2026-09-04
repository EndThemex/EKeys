#include "avatar_engine.h"
#include "avatar_shapes.h"
namespace eavatar
{
    void AvatarEngine::begin()
    {
        frame_ = AvatarFrame{};
        state_ = target_state_ = AvatarState::Idle;
        expression_ = AvatarExpression::Neutral;
        state_start_ = transition_start_ = 0;
        last_ms_ = 0;
        blinking_ = false;
        next_blink_ = 0;
        auto_hold_ = 5.0f;
        auto_index_ = 0;
        auto_showing_ = false;
        target_gaze_x_ = target_gaze_y_ = 0;
        speaking_level_ = 0;
        speak_s_ = 0;
    }
    void AvatarEngine::event(AvatarEvent e)
    {
        auto_hold_ = 6.0f;
        switch (e)
        {
        case AvatarEvent::WakeUp:
            setState(AvatarState::Idle);
            setExpression(AvatarExpression::Neutral);
            break;
        case AvatarEvent::StartListening:
            setState(AvatarState::Idle);
            setExpression(AvatarExpression::Attentive);
            break;
        case AvatarEvent::StopListening:
            setState(AvatarState::Idle);
            setExpression(AvatarExpression::Neutral);
            break;
        case AvatarEvent::StartThinking:
            setState(AvatarState::Thinking);
            setExpression(AvatarExpression::Neutral);
            break;
        case AvatarEvent::StartSpeaking:
            setState(AvatarState::Wide);
            setExpression(AvatarExpression::Happy);
            break;
        case AvatarEvent::Success:
            setState(AvatarState::Nod);
            setExpression(AvatarExpression::Happy);
            break;
        case AvatarEvent::Error:
            setState(AvatarState::ShakeHead);
            setExpression(AvatarExpression::Confused);
            break;
        case AvatarEvent::Sleep:
            setState(AvatarState::Sleep);
            setExpression(AvatarExpression::Sleepy);
            break;
        case AvatarEvent::UserInteraction:
            setState(AvatarState::Wink);
            break;
        }
    }
    void AvatarEngine::setState(AvatarState s)
    {
        auto_showing_ = false; /* 状态所有权移交外部；autoPlayTick 自己设状态后会重新置位 */
        if (s == state_)
            return;
        state_ = target_state_ = s;
        transition_start_ = last_ms_;
        state_start_ = last_ms_;
    }
    void AvatarEngine::setExpression(AvatarExpression e) { expression_ = e; }
    void AvatarEngine::setGaze(float x, float y)
    {
        target_gaze_x_ = clamp(x, -1, 1);
        target_gaze_y_ = clamp(y, -1, 1);
    }
    void AvatarEngine::setSpeakingLevel(float v) { speaking_level_ = clamp01(v); }
    void AvatarEngine::applyExpression(AvatarExpression e, AvatarFrame &o)
    {
        o.gaze_yaw = 28.49f;
        o.gaze_pitch = 28.62f;
        o.gaze_roll = -13;
        o.eye_split = 15.46f;
        o.eyes[0] = o.eyes[1] = EyeFrame{};
        switch (e)
        {
        case AvatarExpression::Attentive:
            o.gaze_yaw = 4;
            o.gaze_pitch = 5;
            o.gaze_roll = -4;
            o.eye_split = 16;
            o.eyes[0].width = o.eyes[1].width = .21f;
            o.eyes[0].height = o.eyes[1].height = .44f;
            break;
        case AvatarExpression::Surprised:
            o.gaze_yaw = 3;
            o.gaze_pitch = -3;
            o.gaze_roll = 0;
            o.eye_split = 19;
            o.eyes[0].width = o.eyes[1].width = .45f;
            o.eyes[0].height = o.eyes[1].height = .47f;
            break;
        case AvatarExpression::Happy:
            o.gaze_yaw = 5;
            o.gaze_pitch = 9;
            o.gaze_roll = 0;
            o.eye_split = 17;
            o.eyes[0].width = o.eyes[1].width = .27f;
            o.eyes[0].height = o.eyes[1].height = .17f;
            o.eyes[0].tilt = 14;
            o.eyes[1].tilt = -14;
            break;
        case AvatarExpression::Angry:
            o.gaze_yaw = 3;
            o.gaze_pitch = 7;
            o.gaze_roll = 0;
            o.eye_split = 17;
            o.eyes[0].width = o.eyes[1].width = .34f;
            o.eyes[0].height = o.eyes[1].height = .15f;
            o.eyes[0].tilt = 30;
            o.eyes[1].tilt = -30;
            break;
        case AvatarExpression::Sad:
            o.gaze_yaw = 3;
            o.gaze_pitch = -13;
            o.eye_split = 16;
            o.eyes[0].width = o.eyes[1].width = .22f;
            o.eyes[0].height = o.eyes[1].height = .40f;
            o.eyes[0].tilt = -28;
            o.eyes[1].tilt = 28;
            break;
        case AvatarExpression::Confused:
            o.gaze_yaw = -14;
            o.gaze_pitch = 3;
            o.gaze_roll = 8;
            o.eye_split = 16.5f;
            o.eyes[0].width = .20f;
            o.eyes[0].height = .44f;
            o.eyes[0].tilt = -18;
            o.eyes[1].width = .28f;
            o.eyes[1].height = .17f;
            o.eyes[1].tilt = -14;
            break;
        case AvatarExpression::Sleepy:
            o.gaze_yaw = 6;
            o.gaze_pitch = -9;
            o.gaze_roll = -3;
            o.eye_split = 16;
            o.eyes[0].open = o.eyes[1].open = .42f;
            break;
        default:
            break;
        }
    }
    void AvatarEngine::gazeFromXY(float x, float y, AvatarFrame &o)
    {
        o.gaze_yaw += x * 22.0f;
        o.gaze_pitch += -y * 18.0f;
        o.gaze_roll += x * 2.5f;
    }
    void AvatarEngine::sampleState(AvatarState s, float t, AvatarFrame &o)
    {
        float breath = 1.0f + sinf((t / 3.4f) * EA_TAU) * .01f;
        o.body_scale = breath;
        o.body_rotation = 0;
        o.accent_alpha = 0;
        o.shake_x = 0;
        if (s == AvatarState::Idle)
        {
            o.gaze_yaw += noise(t, 5.3f, 2.9f) * 2.2f;
            o.gaze_pitch += noise(t, 6.1f, 4.4f) * 1.6f;
        }
        switch (s)
        {
        case AvatarState::Thinking:
        {
            float p = fmodf(t, 2.6f);
            o.body_scale = 1.0f;
            o.gaze_yaw += noise(t, 3.7f, 2.1f) * 8;
            o.gaze_pitch += noise(t, 4.3f, .7f) * 6;
            o.accent_alpha = .65f;
            float q = fmodf(p / 1.5f, 1);
            o.accent_x = (q - .5f) * 1.35f;
            break;
        }
        case AvatarState::Wink:
            o.eyes[1].width = .447f;
            o.eyes[1].height = .089f;
            o.eyes[1].open = 1;
            o.eyes[0].width = .236f;
            o.eyes[0].height = .464f;
            o.gaze_yaw = -5.37f;
            o.gaze_pitch = 4.55f;
            o.gaze_roll = 6.7f;
            o.eye_split = 16.25f;
            break;
        case AvatarState::Wide:
            o.eyes[0].width = o.eyes[1].width = .356f;
            o.eyes[0].height = o.eyes[1].height = .875f;
            o.gaze_yaw = 6.92f;
            o.gaze_pitch = -21.96f;
            o.gaze_roll = 11.6f;
            o.eye_split = 18.43f;
            /* 音量平滑值驱动眼睛高度/身体起伏，充当无嘴设计的"嘴型"代理 */
            o.eyes[0].height *= 0.85f + 0.30f * speak_s_;
            o.eyes[1].height *= 0.85f + 0.30f * speak_s_;
            o.body_y = -1.5f * speak_s_;
            break;
        case AvatarState::Alert:
            o.eye_split = 17;
            o.gaze_yaw = 3;
            o.gaze_pitch = 7;
            o.shake_x = sinf(t * EA_TAU * 2.5f) * 1.0f;
            break;
        case AvatarState::Notify:
            o.gaze_yaw = -21.94f;
            o.gaze_pitch = -5.82f;
            o.gaze_roll = -12.2f;
            o.eye_split = 18.89f;
            o.eyes[0].width = o.eyes[1].width = .505f;
            o.eyes[0].height = o.eyes[1].height = .498f;
            o.accent_alpha = .9f;
            o.accent_x = .72f;
            o.accent_y = -.55f;
            break;
        case AvatarState::Exclaim:
            o.body_scale = .75f;
            o.body_y = .0f;
            o.eyes[0].alpha = o.eyes[1].alpha = 0;
            break;
        case AvatarState::Sleep:
            o.body_scale = .16f;
            o.eyes[0].alpha = o.eyes[1].alpha = 0;
            o.body_y = .11f + sinf(t * EA_TAU / .6f) * .19f;
            break;
        case AvatarState::Burst:
        {
            float c = 1 - .834f * easeOutCubic(clamp01(t / .7f));
            float r = easeOutCubic(clamp01((t - 1.7f) / .7f));
            o.body_scale = c + (1 - c) * r;
            o.accent_alpha = .55f * (1 - o.body_scale);
            break;
        }
        case AvatarState::Comet:
        {
            float c = 1 - .965f * easeOutCubic(clamp01(t / .55f));
            float r = easeOutCubic(clamp01((t - 1.85f) / .6f));
            o.body_scale = c + (1 - c) * r;
            o.accent_alpha = clamp01((t - .15f) / .25f) * clamp01((1.95f - t) / .3f);
            o.accent_x = .45f - t * .42f;
            break;
        }
        case AvatarState::Egg:
            /* 不倒翁式慢摇（rotation 经 LUT 相位偏移实现，仅非圆 profile 可见） */
            o.body_rotation = 10.0f * sinf(t * EA_TAU / 1.2f);
            o.body_x = 1.5f * sinf(t * EA_TAU / 1.2f);
            break;
        case AvatarState::Hexagon:
            /* 缓慢来回转，展示棱角 */
            o.body_rotation = 9.0f * sinf(t * EA_TAU / 2.0f);
            break;
        case AvatarState::Play:
            break;
        /* ---- 点头：身体和视线同步俯仰，约 2 下（Success 事件的反馈） ---- */
        case AvatarState::Nod:
        {
            float w = sinf(t * EA_TAU / 0.7f);
            o.body_y = 2.4f * w;
            o.gaze_pitch += 6.0f * w;
            break;
        }
        /* ---- 摇头：阻尼衰减横移 + 视线反向微滚（Error 事件的反馈） ---- */
        case AvatarState::ShakeHead:
        {
            float d = clamp01(1.0f - t / 1.6f);
            float w = sinf(t * EA_TAU * 2.2f);
            o.shake_x = 3.2f * d * w;
            o.gaze_yaw += -4.0f * d * w;
            o.gaze_roll += 2.5f * d * w;
            break;
        }
        /* ---- 弹跳：下蹲(0~0.3s) → 腾空拉伸(0.3~0.9s) → 落地压缩(0.9~1.15s) → 回稳 ---- */
        case AvatarState::Bounce:
            if (t < 0.3f)
            {
                float u = t / 0.3f;
                o.body_squash = 1.0f - 0.20f * u;
                o.body_y = 2.0f * u;
            }
            else if (t < 0.9f)
            {
                float u = (t - 0.3f) / 0.6f;
                o.body_y = 2.0f - 14.0f * sinf(u * EA_PI);
                o.body_squash = 1.0f + 0.12f * sinf(u * EA_PI);
                o.eyes[0].height *= 1.15f;
                o.eyes[1].height *= 1.15f;
            }
            else if (t < 1.15f)
            {
                float u = (t - 0.9f) / 0.25f;
                o.body_squash = 1.0f - 0.15f * sinf(u * EA_PI);
            }
            break;
        /* ---- 摇摆：2 个周期横移，圆身上 rotation 不可见，靠视线反向微滚出效果 ---- */
        case AvatarState::Sway:
        {
            float w = sinf(t * EA_TAU / 1.5f);
            o.body_x = 3.0f * w;
            o.body_rotation = 7.0f * w;
            o.gaze_roll += -4.0f * w;
            break;
        }
        /* ---- 左顾右盼：中→左→右→中，只有眼睛在动（skill §9：生命力主要来自眼睛） ---- */
        case AvatarState::LookAround:
            if (t < 0.5f)
                o.gaze_yaw += -18.0f * smoothstep(t / 0.5f);
            else if (t < 1.1f)
                o.gaze_yaw += -18.0f;
            else if (t < 1.6f)
                o.gaze_yaw += -18.0f + 36.0f * smoothstep((t - 1.1f) / 0.5f);
            else if (t < 2.2f)
                o.gaze_yaw += 18.0f;
            else
                o.gaze_yaw += 18.0f * (1.0f - smoothstep((t - 2.2f) / 0.6f));
            break;
        /* ---- 心跳：0.8s 一个 lub-dub 双跳 ---- */
        case AvatarState::Heartbeat:
        {
            float tt = fmodf(t, 0.8f);
            float lub = tt < 0.2f ? sinf(tt / 0.2f * EA_PI) : 0.0f;
            float dub = (tt >= 0.18f && tt < 0.4f) ? sinf((tt - 0.18f) / 0.22f * EA_PI) : 0.0f;
            o.body_scale = 1.0f + 0.05f * lub + 0.035f * dub;
            break;
        }
        /* ---- 卫星环绕：accent 点绕身体转，视线跟着走（椭圆轨道压低 y 适配宽屏） ---- */
        case AvatarState::Orbit:
        {
            float a = t * EA_TAU / 1.6f;
            o.accent_alpha = 0.8f;
            o.accent_x = 0.78f * cosf(a);
            o.accent_y = 0.43f * sinf(a);
            o.gaze_yaw += 9.0f * cosf(a);
            o.gaze_pitch += 6.0f * sinf(a);
            break;
        }
        default:
            break;
        }
    }
    void AvatarEngine::updateBlink(float t, AvatarFrame &o)
    {
        if (state_ != AvatarState::Idle && state_ != AvatarState::Thinking)
            return;
        if (!blinking_ && t >= next_blink_)
        {
            blinking_ = true;
            blink_start_ = last_ms_;
            next_blink_ = t + 1.9f + hash01((uint32_t)last_ms_) * 2.7f;
        }
        if (!blinking_)
            return;
        float k = (last_ms_ - blink_start_) / 180.0f;
        float lid = k < .45f ? 1 - k / .45f : (k < 1 ? (k - .45f) / .55f : 1);
        if (k >= 1)
        {
            blinking_ = false;
            return;
        }
        float scale = .06f + .94f * clamp01(lid);
        o.eyes[0].open *= scale;
        o.eyes[1].open *= scale;
    }
    void AvatarEngine::update(uint32_t now)
    {
        uint32_t prev_ms = last_ms_;
        last_ms_ = now;
        float dt = (now > prev_ms) ? (now - prev_ms) / 1000.0f : 0.0f;
        autoPlayTick(now, dt);
        if (state_start_ == 0)
        {
            state_start_ = now;
            transition_start_ = now;
            next_blink_ = (float)now / 1000.0f + 2.2f;
        }
        float t = (now - state_start_) / 1000.0f;
        AvatarFrame target{};
        applyExpression(expression_, target);
        sampleState(state_, t, target);
        gazeFromXY(target_gaze_x_, target_gaze_y_, target);
        ShapeProfile sp;
        ShapeLibrary::profileFor(state_, sp);
        for (int i = 0; i < 64; i++)
            target.body_radius[i] = sp.radius[i];
        float k = easeOutCubic(clamp01((now - transition_start_) / 450.0f));
        for (int i = 0; i < 2; i++)
        {
            target.eyes[i].alpha *= target.alpha;
            frame_.eyes[i].width = lerp(frame_.eyes[i].width, target.eyes[i].width, k);
            frame_.eyes[i].height = lerp(frame_.eyes[i].height, target.eyes[i].height, k);
            frame_.eyes[i].open = lerp(frame_.eyes[i].open, target.eyes[i].open, k);
            frame_.eyes[i].tilt = lerp(frame_.eyes[i].tilt, target.eyes[i].tilt, k);
            frame_.eyes[i].alpha = lerp(frame_.eyes[i].alpha, target.eyes[i].alpha, k);
        }
        frame_.body_scale = lerp(frame_.body_scale, target.body_scale, k);
        for (int i = 0; i < 64; i++)
            frame_.body_radius[i] = lerp(frame_.body_radius[i], target.body_radius[i], k);
        frame_.body_x = lerp(frame_.body_x, target.body_x, k);
        frame_.body_y = lerp(frame_.body_y, target.body_y, k);
        frame_.gaze_yaw = lerp(frame_.gaze_yaw, target.gaze_yaw, k);
        frame_.gaze_pitch = lerp(frame_.gaze_pitch, target.gaze_pitch, k);
        frame_.gaze_roll = lerp(frame_.gaze_roll, target.gaze_roll, k);
        frame_.eye_split = lerp(frame_.eye_split, target.eye_split, k);
        frame_.shake_x = target.shake_x;
        frame_.accent_x = target.accent_x;
        frame_.accent_y = target.accent_y;
        frame_.accent_alpha = target.accent_alpha;
        frame_.speaking_level = speaking_level_;
        updateBlink((float)now / 1000.0f, frame_);
    }

    /* ---- 自动播放节目单 ----
     * Idle 停留几秒后轮流表演一个状态，表演完回 Idle 休息，循环往复。
     * 节奏由 hash01(now) 伪随机打散，避免机械感。
     * 刻意不含 Sleep / Alert / Exclaim：无事件时自动触发语义不对
     * （Sleep=休眠、Alert=报警抖动、Exclaim=隐藏眼睛的惊叹）。 */
    static const AvatarState AUTO_SHOWS[] = {
        AvatarState::Wink,       /* 单眼眨：1.6s */
        AvatarState::LookAround, /* 左顾右盼：2.8s */
        AvatarState::Hexagon,    /* 变六边形+慢转：4.0s */
        AvatarState::Bounce,     /* 弹跳（压扁拉伸）：1.8s */
        AvatarState::Egg,        /* 变蛋形+摇晃：4.0s */
        AvatarState::Orbit,      /* 卫星环绕：3.2s */
        AvatarState::Notify,     /* 看左上+高亮点：2.4s */
        AvatarState::Heartbeat,  /* 心跳双跳：1.6s */
        AvatarState::Thinking,   /* 思考（高亮点来回）：3.5s */
        AvatarState::Sway,       /* 左右摇摆：3.0s */
        AvatarState::Burst,      /* 缩放脉冲：2.4s */
        AvatarState::Comet,      /* 彗星划过：2.4s */
    };
    static const float AUTO_SHOW_DUR[] = {1.6f, 2.8f, 4.0f, 1.8f, 4.0f, 3.2f, 2.4f, 1.6f, 3.5f, 3.0f, 2.4f, 2.4f};
    static const int AUTO_SHOW_N = (int)(sizeof(AUTO_SHOWS) / sizeof(AUTO_SHOWS[0]));
    static_assert((int)(sizeof(AUTO_SHOW_DUR) / sizeof(AUTO_SHOW_DUR[0])) == AUTO_SHOW_N,
                  "AUTO_SHOW_DUR must match AUTO_SHOWS");

    /* 外部瞬态反馈状态自动收回的停留时长（秒）。
     * Error→Alert / Success→Notify / UserInteraction→Wink 按 skill §24/§25
     * 应是"短暂反应后回 Idle"；应用层若不发后续事件，引擎兜底收回。 */
    static const float AUTO_TRANSIENT_HOLD_S = 2.5f;

    void AvatarEngine::autoPlayTick(uint32_t now, float dt)
    {
        /* ---- 外部事件设置的语义状态：自动播放不接管 ---- */
        if (!auto_showing_ && state_ != AvatarState::Idle)
        {
            bool transient = (state_ == AvatarState::Alert || state_ == AvatarState::Notify ||
                              state_ == AvatarState::Wink);
            if (transient)
            {
                /* 瞬态反馈：停留够久自动收回 Idle */
                float stayed = (last_ms_ - state_start_) / 1000.0f;
                if (stayed >= AUTO_TRANSIENT_HOLD_S)
                {
                    setState(AvatarState::Idle);
                    setExpression(AvatarExpression::Neutral);
                    auto_hold_ = 6.0f; /* 收回后先安静一会儿再考虑表演 */
                }
            }
            /* 持续语义状态（Sleep/Thinking/Wide=Speaking 等）：等对应收尾事件
             * （WakeUp/StopListening/TTS done），自动播放完全不介入。 */
            return;
        }

        if (auto_hold_ > 0)
            auto_hold_ -= dt;
        if (auto_hold_ > 0)
            return;

        if (state_ != AvatarState::Idle)
        {
            /* 自己的节目演完 → 回 Idle 休息 4~9s */
            setState(AvatarState::Idle);
            setExpression(AvatarExpression::Neutral);
            auto_showing_ = false;
            auto_hold_ = 4.0f + hash01((uint32_t)now) * 5.0f;
            return;
        }

        /* 仅在"纯待机"（Neutral 表情）时表演；Attentive=聆听中（skill §8：
         * 聆听要专注不要 hyperactive），保持安静，稍后再查。 */
        if (expression_ != AvatarExpression::Neutral)
        {
            auto_hold_ = 2.0f;
            return;
        }

        /* 休息结束 → 表演下一个节目（setState 会清 auto_showing_，随后置位） */
        uint8_t i = auto_index_ % (uint8_t)AUTO_SHOW_N;
        auto_index_++;
        setState(AUTO_SHOWS[i]);
        auto_showing_ = true;
        auto_hold_ = AUTO_SHOW_DUR[i];
    }
}
