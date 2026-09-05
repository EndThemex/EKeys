/*
 * BatteryMonitor.cpp
 *
 * ESP32-S3 ADC1 通道读取 + 滑动平均 + 1:1 分压还原 + 电压→百分比。
 *
 * 关键点：
 *   - 使用 analogReadMilliVolts()（内部已校准），无需手动换算
 *   - ADC 量程 0~3.3V（默认 ATTEN_DB_12），可覆盖 1S 锂电池最高 4.2V 经 1:1 分压后的 2.1V
 *   - 分压比 = 1 / (R2 / (R1 + R2))，R1=R2=200k → Vbattery = Vadc * 2
 */

#include "BatteryMonitor.h"

#include <Arduino.h>

#include "logging/LogManager.h"

namespace ekeys
{

    namespace
    {
        /*
         * 1:1 分压（200k + 200k），将 ADC 端电压还原为电池端电压。
         */
        constexpr float kDividerRatio = 2.0f;

        /*
         * 锂电池 1S 电压 → 百分比（线性分段，参考常见放电曲线）。
         * 低于 kCutoffMv 直接返回 0，高于 kFullMv 返回 100。
         */
        constexpr uint16_t kFullMv = 4200;
        constexpr uint16_t kHalfMv = 3700;
        constexpr uint16_t kLowMv = 3300;
        constexpr uint16_t kCutoffMv = 3000;

        /* 滑动平均窗口大小（次数） */
        constexpr uint8_t kAvgSamples = 8;
    } // namespace

    BatteryMonitor &BatteryMonitor::instance()
    {
        static BatteryMonitor inst;
        return inst;
    }

    void BatteryMonitor::begin()
    {
        /*
         * analogReadMilliVolts() 内部要求 pin 已被 analogSetPinAttenuation()
         * 或 analogSetAttenuation() 配置。这里显式设置 11dB（≈ 0~3.3V 量程），
         * 保证 4.2V 电池经 1:1 分压后的 2.1V 落在量程中段。
         */
        analogSetPinAttenuation(kPinBatteryAdc, ADC_11db);
        LOG_INFO("BATT", "BatteryMonitor ready (gpio=%d, ratio=%.2f)",
                 static_cast<int>(kPinBatteryAdc),
                 static_cast<double>(kDividerRatio));
    }

    uint16_t BatteryMonitor::adcMilliVoltsAvg_()
    {
        uint32_t sum = 0;
        for (uint8_t i = 0; i < kAvgSamples; ++i)
        {
            sum += analogReadMilliVolts(kPinBatteryAdc);
        }
        return static_cast<uint16_t>(sum / kAvgSamples);
    }

    uint16_t BatteryMonitor::readMilliVolts()
    {
        const uint16_t adc_mv = adcMilliVoltsAvg_();
        const float vbat_mv = static_cast<float>(adc_mv) * kDividerRatio;
        return static_cast<uint16_t>(vbat_mv);
    }

    uint8_t BatteryMonitor::readPercent()
    {
        const uint16_t mv = readMilliVolts();

        if (mv >= kFullMv)
        {
            return 100;
        }
        if (mv <= kCutoffMv)
        {
            return 0;
        }

        /*
         * 分段线性映射：
         *   [kCutoffMv, kLowMv]  → 0~10%
         *   [kLowMv,     kHalfMv] → 10~50%
         *   [kHalfMv,    kFullMv] → 50~100%
         */
        if (mv < kLowMv)
        {
            const uint32_t span = kLowMv - kCutoffMv;     // 300
            const uint32_t delta = mv - kCutoffMv;        // 0~300
            return static_cast<uint8_t>((delta * 10U) / span);
        }
        if (mv < kHalfMv)
        {
            const uint32_t span = kHalfMv - kLowMv;       // 400
            const uint32_t delta = mv - kLowMv;          // 0~400
            return static_cast<uint8_t>(10U + (delta * 40U) / span);
        }
        const uint32_t span = kFullMv - kHalfMv;          // 500
        const uint32_t delta = mv - kHalfMv;             // 0~500
        return static_cast<uint8_t>(50U + (delta * 50U) / span);
    }

} // namespace ekeys
