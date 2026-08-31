/*
 * RotaryEncoder.h
 *
 * EC11 旋钮（PINOUT §2.3）：A/B 相位 → 左 / 右；SW 单击 → 进入、
 * 双击 → 返回。回调返回 LV_KEY_LEFT / LV_KEY_RIGHT / LV_KEY_ENTER /
 * LV_KEY_ESC，由 MainTask 转成 DisplayMessageType::ActionInput。
 *
 * 移植自参考工程 RotaryEncoder（ESP32Encoder PCNT + OneButton），
 * 引脚改为 PinMap.h 的 kPinEc11*。
 */

#ifndef EKEYS_INPUT_ROTARY_ENCODER_H
#define EKEYS_INPUT_ROTARY_ENCODER_H

#include <Arduino.h>
#include <OneButton.h>
#include <ESP32Encoder.h>
#include <lvgl.h>

#include <functional>

namespace ekeys
{

    class RotaryEncoder
    {
    public:
        using EncoderCallback = std::function<void(uint8_t key)>;

        RotaryEncoder();

        void begin();
        void loop();
        void setCallback(EncoderCallback cb) { callback_ = cb; }

    private:
        void checkRotation();

        static void handleClick(void *context);
        static void handleDoubleClick(void *context);

        ESP32Encoder encoder_;
        OneButton button_;
        EncoderCallback callback_;

        /* 旋转去抖 / 方向判定状态 */
        unsigned long lastRotationTime_{0};
        bool rotationActive_{false};
        int lastDirection_{0}; /* 0=无, 1=右, -1=左 */
        int accumulatedSteps_{0};
        int lastValue_{0};
        bool encoderEnabled_{false};
    };

} // namespace ekeys

#endif // EKEYS_INPUT_ROTARY_ENCODER_H
