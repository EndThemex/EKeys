/*
 * PowerConfig.h
 *
 * 电源模式（DeviceSettings.power_mode）行为参数，与 UI / App 协议取值一致：
 *   0 正常    —— 不改变背光（与旧行为一致）
 *   1 低功耗  —— 无操作超时后降低屏幕亮度
 *   2 浅休眠  —— 无操作超时后关闭屏幕背光
 *   3 深休眠  —— 暂未实现，按 0 处理
 *
 * 空闲判定在 DisplayTask（Core 0）完成：旋钮 / 矩阵键任意输入都算操作。
 */

#ifndef EKEYS_CONFIG_POWER_CONFIG_H
#define EKEYS_CONFIG_POWER_CONFIG_H

#include <stdint.h>

namespace ekeys
{

    /* 仅列出生效的模式（0 正常 = 不处理；3 深休眠暂未实现） */
    constexpr uint8_t kPowerModeLowPower = 1;
    constexpr uint8_t kPowerModeLightSleep = 2;

    /* 无操作超时，单位 ms（旋钮 / 矩阵键均视为操作） */
    constexpr uint32_t kPowerIdleTimeoutMs = 30000;

    /* 模式 1 超时后的屏幕亮度百分比（Backlight 内部下限 5%） */
    constexpr uint8_t kPowerIdleDimPercent = 10;

} // namespace ekeys

#endif // EKEYS_CONFIG_POWER_CONFIG_H
