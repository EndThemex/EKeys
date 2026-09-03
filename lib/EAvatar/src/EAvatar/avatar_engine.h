#pragma once
#include "avatar_types.h"
#include "avatar_math.h"
namespace eavatar {
class AvatarEngine {
public:
 void begin(); void update(uint32_t now_ms); void event(AvatarEvent); void setState(AvatarState); void setExpression(AvatarExpression); void setGaze(float x,float y); void setSpeakingLevel(float);
 AvatarState state()const{return state_;} AvatarFrame frame()const{return frame_;}
private:
 AvatarState state_=AvatarState::Idle,target_state_=AvatarState::Idle; AvatarExpression expression_=AvatarExpression::Neutral;
 AvatarFrame frame_{};
 uint32_t state_start_ = 0, transition_start_ = 0, last_ms_ = 0, blink_start_ = 0;
 /* next_blink_ 单位是"秒"（updateBlink 以秒比较）；旧版误用毫秒值 2200 导致 36 分钟才眨一次眼 */
 float next_blink_ = 0;
 /* 自动播放：Idle 停留 auto_hold_ 秒后轮流表演一个状态，表演完回 Idle 休息 */
 float auto_hold_ = 5.0f;
 uint8_t auto_index_ = 0;
 bool blinking_ = false;
 float target_gaze_x_=0,target_gaze_y_=0,speaking_level_=0;
 void applyExpression(AvatarExpression, AvatarFrame &);
 void sampleState(AvatarState, float, AvatarFrame &);
 void updateBlink(float, AvatarFrame &);
 void gazeFromXY(float, float, AvatarFrame &);
 void autoPlayTick(uint32_t now, float dt);
};
}
