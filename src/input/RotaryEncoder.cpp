/*
 * RotaryEncoder.cpp
 *
 * 旋转判定沿用参考工程策略：
 *   - 半四分计数（attachHalfQuad），滤波 1023；
 *   - 累计步数 ≥ kEncoderStepThreshold 或换向 / 超时才触发一次，
 *     避免抖动产生连续左右键；
 *   - 停转 kRotationTimeout 后复位，下次旋转重新累计。
 */

#include "RotaryEncoder.h"

#include "hardware/PinMap.h"
#include "logging/LogManager.h"

namespace ekeys
{

    namespace
    {
        constexpr unsigned long kRotationTimeout = 500; /* ms */
        constexpr int kEncoderStepThreshold = 2;
    } // namespace

    RotaryEncoder::RotaryEncoder()
        : button_(kPinEc11Sw, true), /* SW 内部上拉 */
          callback_(nullptr)
    {
    }

    void RotaryEncoder::begin()
    {
        button_.attachClick(handleClick, this);
        button_.attachDoubleClick(handleDoubleClick, this);

        ESP32Encoder::isrServiceCpuCore = ISR_CORE_USE_DEFAULT;
        ESP32Encoder::useInternalWeakPullResistors = UP;
        /* A 相作 CLK，B 相作 DT；实测方向相反时对调两参数即可 */
        encoder_.attachHalfQuad(kPinEc11A, kPinEc11B);
        encoder_.setFilter(1023);
        encoder_.setCount(0);
        encoderEnabled_ = true;

        LOG_INFO("ENC", "rotary encoder ready (SW=%u A=%u B=%u)",
                 kPinEc11Sw, kPinEc11A, kPinEc11B);
    }

    void RotaryEncoder::loop()
    {
        button_.tick();
        if (encoderEnabled_)
        {
            checkRotation();
        }
    }

    void RotaryEncoder::checkRotation()
    {
        const int currentValue = static_cast<int>(encoder_.getCount());
        const int delta = currentValue - lastValue_;
        const unsigned long now = millis();

        if (delta != 0)
        {
            const int currentDirection = (delta > 0) ? 1 : -1;
            accumulatedSteps_ += (delta > 0) ? delta : -delta;

            if (!rotationActive_ ||
                currentDirection != lastDirection_ ||
                (now - lastRotationTime_) > kRotationTimeout ||
                accumulatedSteps_ >= kEncoderStepThreshold)
            {
                if (callback_ && accumulatedSteps_ >= kEncoderStepThreshold)
                {
                    callback_(currentDirection > 0 ? LV_KEY_RIGHT
                                                   : LV_KEY_LEFT);
                }

                rotationActive_ = true;
                lastDirection_ = currentDirection;
                accumulatedSteps_ = 0;
            }

            lastRotationTime_ = now;
            lastValue_ = currentValue;
        }
        else if (rotationActive_ &&
                 (now - lastRotationTime_) > kRotationTimeout)
        {
            rotationActive_ = false;
            accumulatedSteps_ = 0;
        }
    }

    void RotaryEncoder::handleClick(void *context)
    {
        auto *self = static_cast<RotaryEncoder *>(context);
        if (self->callback_ != nullptr)
        {
            self->callback_(LV_KEY_ENTER);
        }
    }

    void RotaryEncoder::handleDoubleClick(void *context)
    {
        auto *self = static_cast<RotaryEncoder *>(context);
        if (self->callback_ != nullptr)
        {
            self->callback_(LV_KEY_ESC);
        }
    }

} // namespace ekeys
