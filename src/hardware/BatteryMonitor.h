/*
 * BatteryMonitor.h
 *
 * 通过 ADC1 通道读取分压后的电池电压。
 *
 * 硬件：GPIO4 (kPinBatteryAdc) 接两个 200kΩ 电阻构成 1:1 分压，
 *       ADC 端电压 = Vbattery / 2。采样后乘 2 还原电池电压。
 *
 * 锂电池 1S 放电曲线近似（实测可调）：
 *   4.20V → 100%
 *   3.70V →  50%
 *   3.30V →  10%
 *   3.00V →   0%
 *
 * 用法：
 *   BatteryMonitor::instance().begin();     // setup()
 *   const uint8_t pct = BatteryMonitor::instance().readPercent(); // 周期调用
 */

#ifndef EKEYS_HARDWARE_BATTERY_MONITOR_H
#define EKEYS_HARDWARE_BATTERY_MONITOR_H

#include <stdint.h>

#include "hardware/PinMap.h"

namespace ekeys
{

    class BatteryMonitor
    {
    public:
        static BatteryMonitor &instance();

        /* 配置 ADC 通道 + 衰减。必须在使用 read*() 前调用一次。 */
        void begin();

        /*
         * 读取电池电量百分比 0~100。
         * 内部做 8 次滑动平均；D7 修复：内置 5s TTL 缓存，
         * 5s 内重复调用直接返回上次结果，避免上游误改为"每 tick 调用"时 ADC 高占用。
         */
        uint8_t readPercent();

        /* 读取电池电压（mV），便于日志 / 调试。同样 5s 内部节流。 */
        uint16_t readMilliVolts();

    private:
        BatteryMonitor() = default;

        uint16_t adcMilliVoltsAvg_();

        /* D7：缓存最近一次 ADC 采样值 + 时间戳 */
        static constexpr uint32_t kReadCacheTtlMs = 5000;
        bool cached_mv_valid_ = false;
        uint32_t cached_mv_ms_ = 0;
        uint16_t cached_mv_ = 0;
    };

} // namespace ekeys

#endif // EKEYS_HARDWARE_BATTERY_MONITOR_H
