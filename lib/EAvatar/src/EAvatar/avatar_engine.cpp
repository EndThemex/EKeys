#include "avatar_engine.h"
#include "avatar_shapes.h"
namespace eavatar {
void AvatarEngine::begin(){frame_=AvatarFrame{}; state_=target_state_=AvatarState::Idle; expression_=AvatarExpression::Neutral; state_start_=transition_start_=0; last_ms_=0; blinking_=false; next_blink_=0; auto_hold_=5.0f;}
void AvatarEngine::event(AvatarEvent e){auto_hold_=6.0f;switch(e){case AvatarEvent::WakeUp:setState(AvatarState::Idle);setExpression(AvatarExpression::Neutral);break;case AvatarEvent::StartListening:setState(AvatarState::Idle);setExpression(AvatarExpression::Attentive);break;case AvatarEvent::StopListening:setState(AvatarState::Idle);setExpression(AvatarExpression::Neutral);break;case AvatarEvent::StartThinking:setState(AvatarState::Thinking);setExpression(AvatarExpression::Neutral);break;case AvatarEvent::StartSpeaking:setState(AvatarState::Wide);setExpression(AvatarExpression::Happy);break;case AvatarEvent::Success:setState(AvatarState::Notify);setExpression(AvatarExpression::Happy);break;case AvatarEvent::Error:setState(AvatarState::Alert);setExpression(AvatarExpression::Confused);break;case AvatarEvent::Sleep:setState(AvatarState::Sleep);setExpression(AvatarExpression::Sleepy);break;case AvatarEvent::UserInteraction:setState(AvatarState::Wink);break;}}
void AvatarEngine::setState(AvatarState s){if(s==state_)return;state_=target_state_=s;transition_start_=last_ms_;state_start_=last_ms_;}
void AvatarEngine::setExpression(AvatarExpression e){expression_=e;}
void AvatarEngine::setGaze(float x,float y){target_gaze_x_=clamp(x,-1,1);target_gaze_y_=clamp(y,-1,1);}
void AvatarEngine::setSpeakingLevel(float v){speaking_level_=clamp01(v);}
void AvatarEngine::applyExpression(AvatarExpression e,AvatarFrame&o){o.gaze_yaw=28.49f;o.gaze_pitch=28.62f;o.gaze_roll=-13;o.eye_split=15.46f;o.eyes[0]=o.eyes[1]=EyeFrame{};switch(e){case AvatarExpression::Attentive:o.gaze_yaw=4;o.gaze_pitch=5;o.gaze_roll=-4;o.eye_split=16;o.eyes[0].width=o.eyes[1].width=.21f;o.eyes[0].height=o.eyes[1].height=.44f;break;case AvatarExpression::Surprised:o.gaze_yaw=3;o.gaze_pitch=-3;o.gaze_roll=0;o.eye_split=19;o.eyes[0].width=o.eyes[1].width=.45f;o.eyes[0].height=o.eyes[1].height=.47f;break;case AvatarExpression::Happy:o.gaze_yaw=5;o.gaze_pitch=9;o.gaze_roll=0;o.eye_split=17;o.eyes[0].width=o.eyes[1].width=.27f;o.eyes[0].height=o.eyes[1].height=.17f;o.eyes[0].tilt=14;o.eyes[1].tilt=-14;break;case AvatarExpression::Angry:o.gaze_yaw=3;o.gaze_pitch=7;o.gaze_roll=0;o.eye_split=17;o.eyes[0].width=o.eyes[1].width=.34f;o.eyes[0].height=o.eyes[1].height=.15f;o.eyes[0].tilt=30;o.eyes[1].tilt=-30;break;case AvatarExpression::Sad:o.gaze_yaw=3;o.gaze_pitch=-13;o.eye_split=16;o.eyes[0].width=o.eyes[1].width=.22f;o.eyes[0].height=o.eyes[1].height=.40f;o.eyes[0].tilt=-28;o.eyes[1].tilt=28;break;case AvatarExpression::Confused:o.gaze_yaw=-14;o.gaze_pitch=3;o.gaze_roll=8;o.eye_split=16.5f;o.eyes[0].width=.20f;o.eyes[0].height=.44f;o.eyes[0].tilt=-18;o.eyes[1].width=.28f;o.eyes[1].height=.17f;o.eyes[1].tilt=-14;break;case AvatarExpression::Sleepy:o.gaze_yaw=6;o.gaze_pitch=-9;o.gaze_roll=-3;o.eye_split=16;o.eyes[0].open=o.eyes[1].open=.42f;break;default:break;}}
void AvatarEngine::gazeFromXY(float x,float y,AvatarFrame&o){o.gaze_yaw += x*22.0f;o.gaze_pitch += -y*18.0f;o.gaze_roll += x*2.5f;}
void AvatarEngine::sampleState(AvatarState s,float t,AvatarFrame&o){float breath=1.0f+sinf((t/3.4f)*EA_TAU)*.02f;o.body_scale=breath;o.body_rotation=0;o.accent_alpha=0;o.shake_x=0;if(s==AvatarState::Idle){o.gaze_yaw+=noise(t,5.3f,2.9f)*2.2f;o.gaze_pitch+=noise(t,6.1f,4.4f)*1.6f;}switch(s){case AvatarState::Thinking:{float p=fmodf(t,2.6f);o.body_scale=1.0f;o.gaze_yaw += noise(t,3.7f,2.1f)*8;o.gaze_pitch += noise(t,4.3f,.7f)*6;o.accent_alpha=.65f;float q=fmodf(p/1.5f,1);o.accent_x=(q-.5f)*1.35f;break;}case AvatarState::Wink:o.eyes[1].width=.447f;o.eyes[1].height=.089f;o.eyes[1].open=1;o.eyes[0].width=.236f;o.eyes[0].height=.464f;o.gaze_yaw=-5.37f;o.gaze_pitch=4.55f;o.gaze_roll=6.7f;o.eye_split=16.25f;break;case AvatarState::Wide:o.eyes[0].width=o.eyes[1].width=.356f;o.eyes[0].height=o.eyes[1].height=.875f;o.gaze_yaw=6.92f;o.gaze_pitch=-21.96f;o.gaze_roll=11.6f;o.eye_split=18.43f;break;case AvatarState::Alert:o.eye_split=17;o.gaze_yaw=3;o.gaze_pitch=7;o.shake_x=sinf(t*EA_TAU*2.5f)*1.0f;break;case AvatarState::Notify:o.gaze_yaw=-21.94f;o.gaze_pitch=-5.82f;o.gaze_roll=-12.2f;o.eye_split=18.89f;o.eyes[0].width=o.eyes[1].width=.505f;o.eyes[0].height=o.eyes[1].height=.498f;o.accent_alpha=.9f;o.accent_x=.72f;o.accent_y=-.55f;break;case AvatarState::Exclaim:o.body_scale=.75f;o.body_y=.0f;o.eyes[0].alpha=o.eyes[1].alpha=0;break;case AvatarState::Sleep:o.body_scale=.16f;o.eyes[0].alpha=o.eyes[1].alpha=0;o.body_y=.11f+sinf(t*EA_TAU/.6f)*.19f;break;case AvatarState::Burst:{float c=1-.834f*easeOutCubic(clamp01(t/.7f));float r= easeOutCubic(clamp01((t-1.7f)/.7f));o.body_scale=c+(1-c)*r;o.accent_alpha=.55f*(1-o.body_scale);break;}case AvatarState::Comet:{float c=1-.965f*easeOutCubic(clamp01(t/.55f));float r=easeOutCubic(clamp01((t-1.85f)/.6f));o.body_scale=c+(1-c)*r;o.accent_alpha=clamp01((t-.15f)/.25f)*clamp01((1.95f-t)/.3f);o.accent_x=.45f-t*.42f;break;}case AvatarState::Egg:case AvatarState::Hexagon:case AvatarState::Play:break;default:break;}}
void AvatarEngine::updateBlink(float t,AvatarFrame&o){if(state_!=AvatarState::Idle&&state_!=AvatarState::Thinking)return; if(!blinking_ && t>=next_blink_){blinking_=true;blink_start_=last_ms_;next_blink_=t+1.9f+hash01((uint32_t)last_ms_)*2.7f;} if(!blinking_)return;float k=(last_ms_-blink_start_)/180.0f;float lid=k<.45f?1-k/.45f:(k<1?(k-.45f)/.55f:1);if(k>=1){blinking_=false;return;}float scale=.06f+.94f*clamp01(lid);o.eyes[0].open*=scale;o.eyes[1].open*=scale;}
void AvatarEngine::update(uint32_t now){uint32_t prev_ms=last_ms_;last_ms_=now;float dt=(prev_ms!=0&&now>prev_ms)?(now-prev_ms)/1000.0f:0.016f;autoPlayTick(now,dt);if(state_start_==0){state_start_=now;transition_start_=now;next_blink_=(float)now/1000.0f+2.2f;}float t=(now-state_start_)/1000.0f;AvatarFrame target{};applyExpression(expression_,target);sampleState(state_,t,target);gazeFromXY(target_gaze_x_,target_gaze_y_,target); ShapeProfile sp; ShapeLibrary::profileFor(state_,sp); for(int i=0;i<64;i++) target.body_radius[i]=sp.radius[i];float k=easeOutCubic(clamp01((now-transition_start_)/450.0f));for(int i=0;i<2;i++){target.eyes[i].alpha*=target.alpha;frame_.eyes[i].width=lerp(frame_.eyes[i].width,target.eyes[i].width,k);frame_.eyes[i].height=lerp(frame_.eyes[i].height,target.eyes[i].height,k);frame_.eyes[i].open=lerp(frame_.eyes[i].open,target.eyes[i].open,k);frame_.eyes[i].tilt=lerp(frame_.eyes[i].tilt,target.eyes[i].tilt,k);frame_.eyes[i].alpha=lerp(frame_.eyes[i].alpha,target.eyes[i].alpha,k);}frame_.body_scale=lerp(frame_.body_scale,target.body_scale,k); for(int i=0;i<64;i++) frame_.body_radius[i]=lerp(frame_.body_radius[i],target.body_radius[i],k);frame_.body_x=lerp(frame_.body_x,target.body_x,k);frame_.body_y=lerp(frame_.body_y,target.body_y,k);frame_.gaze_yaw=lerp(frame_.gaze_yaw,target.gaze_yaw,k);frame_.gaze_pitch=lerp(frame_.gaze_pitch,target.gaze_pitch,k);frame_.gaze_roll=lerp(frame_.gaze_roll,target.gaze_roll,k);frame_.eye_split=lerp(frame_.eye_split,target.eye_split,k);frame_.shake_x=target.shake_x;frame_.accent_x=target.accent_x;frame_.accent_y=target.accent_y;frame_.accent_alpha=target.accent_alpha;frame_.speaking_level=speaking_level_;updateBlink((float)now/1000.0f,frame_);}

/* ---- 自动播放节目单 ----
 * Idle 停留几秒后轮流表演一个状态，表演完回 Idle 休息，循环往复。
 * 节奏由 hash01(now) 伪随机打散，避免机械感。
 * 刻意不含 Sleep / Alert / Exclaim：无事件时自动触发语义不对
 * （Sleep=休眠、Alert=报警抖动、Exclaim=隐藏眼睛的惊叹）。 */
static const AvatarState AUTO_SHOWS[] = {
    AvatarState::Wink,     /* 单眼眨：1.6s */
    AvatarState::Hexagon,  /* 变六边形：4.0s */
    AvatarState::Wide,     /* 瞪大眼：2.0s */
    AvatarState::Egg,      /* 变蛋形：4.0s */
    AvatarState::Notify,   /* 看左上+高亮点：2.4s */
    AvatarState::Thinking, /* 思考（高亮点来回）：3.5s */
    AvatarState::Burst,    /* 缩放脉冲：2.4s */
    AvatarState::Comet,    /* 彗星划过：2.4s */
};
static const float AUTO_SHOW_DUR[] = {1.6f, 4.0f, 2.0f, 4.0f, 2.4f, 3.5f, 2.4f, 2.4f};
static const int AUTO_SHOW_N = (int)(sizeof(AUTO_SHOWS) / sizeof(AUTO_SHOWS[0]));
static_assert((int)(sizeof(AUTO_SHOW_DUR) / sizeof(AUTO_SHOW_DUR[0])) == AUTO_SHOW_N,
              "AUTO_SHOW_DUR must match AUTO_SHOWS");

void AvatarEngine::autoPlayTick(uint32_t now, float dt)
{
    if (auto_hold_ > 0)
        auto_hold_ -= dt;
    if (auto_hold_ > 0)
        return;

    if (state_ != AvatarState::Idle)
    {
        /* 表演结束（或外部事件留下的状态）→ 回 Idle 休息 4~9s */
        setState(AvatarState::Idle);
        setExpression(AvatarExpression::Neutral);
        auto_hold_ = 4.0f + hash01((uint32_t)now) * 5.0f;
    }
    else
    {
        /* 休息结束 → 表演下一个节目 */
        uint8_t i = auto_index_ % (uint8_t)AUTO_SHOW_N;
        auto_index_++;
        setState(AUTO_SHOWS[i]);
        auto_hold_ = AUTO_SHOW_DUR[i];
    }
}
}
