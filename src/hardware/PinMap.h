/*
 * PinMap.h
 *
 * 集中所有 IO 定义（PINOUT §1.1）。
 *
 * 阶段 01 只引用矩阵 + LCD + 电源 / 旋钮等的引脚；
 * 其它模块按需追加。
 */

#ifndef EKEYS_HARDWARE_PIN_MAP_H
#define EKEYS_HARDWARE_PIN_MAP_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h> // GFX_NOT_DEFINED

/*
 * ------------------------------------------------------------
 * 电源 / 调试
 * ------------------------------------------------------------
 */
constexpr uint8_t kPinBoost5VEnable = 3;
constexpr uint8_t kPinBatteryAdc = 4;

/*
 * ------------------------------------------------------------
 * EC11 旋钮（PINOUT §2.3）
 * ------------------------------------------------------------
 */
constexpr uint8_t kPinEc11Sw = 5;
constexpr uint8_t kPinEc11A = 6;
constexpr uint8_t kPinEc11B = 7;

/*
 * ------------------------------------------------------------
 * 按键矩阵（PINOUT §2.5）
 * ------------------------------------------------------------
 */
constexpr uint8_t kPinMatrixRow0 = 46;
constexpr uint8_t kPinMatrixRow1 = 39;
constexpr uint8_t kPinMatrixRow2 = 38;
constexpr uint8_t kPinMatrixCol0 = 16;
constexpr uint8_t kPinMatrixCol1 = 17;
constexpr uint8_t kPinMatrixCol2 = 18;
constexpr uint8_t kPinMatrixCol3 = 8;

/*
 * ------------------------------------------------------------
 * LCD 屏幕（PINOUT §2.6）
 *
 * 阶段 01 末按用户最新 PINOUT.md：
 *
 *     LCD_DC  = IO42  数据/命令选择
 *     LCD_SDA = IO40  SPI 数据（MOSI）
 *     LCD_SCL = IO41  SPI 时钟（CLK）
 *     LCD_RST        硬件拉低，**未分配引脚**；用 GFX_NOT_DEFINED 跳过软件复位
 * ------------------------------------------------------------
 */
constexpr int kPinLcdBacklight = 1;         // PINOUT §2.6 LCD_BL
constexpr int kPinLcdCs = 2;                // PINOUT §2.6 LCD_CS
constexpr int kPinLcdDc = 42;               // PINOUT §1.1 / §2.6 LCD_DC
constexpr int kPinLcdSclk = 41;             // PINOUT §2.6 LCD_SCL
constexpr int kPinLcdMosi = 40;             // PINOUT §2.6 LCD_SDA
constexpr int kPinLcdRst = GFX_NOT_DEFINED; // 硬件拉低，无引脚

/*
 * ------------------------------------------------------------
 * I2S 音频（PINOUT §2.7）
 *
 * 功放（MAX98357）：BCLK=IO10 / LRCLK=IO9 / DIN=IO11（I2S1 TX）
 *   DIN = 功放数据输入（主控 IO11 → 功放）
 * 麦克风（ICS43434）：SCK=IO13（专用）/ WS=IO12 / SD=IO14（I2S0 RX）
 *   SD = 麦克风数据输出（麦克风 → 主控 IO14），注意与功放 DIN 区分
 *   L/R 引脚接帧时钟（WS 网络）→ 立体声模式，左右时隙均输出同路数据
 *
 * 2026-09-06 原理图确认：ICS43434 的 SCK 走 IO13 专用线，
 * 与功放 BCLK（IO10）物理独立，两个 I2S 外设可同时运行。
 * 2026-09-06 二次更正：SD 与 DIN 数据线对调（旧版误标 IO11←SD / IO14→DIN，
 * 导致录音与播放双向无声）。
 * ------------------------------------------------------------
 */
constexpr uint8_t kPinI2sBclkSpeaker = 10;
constexpr uint8_t kPinI2sLrclkSpeaker = 9;
constexpr uint8_t kPinI2sDataSpeaker = 11; /* DIN：主控输出 → 功放输入 */
constexpr uint8_t kPinI2sMicSck = 13;
constexpr uint8_t kPinI2sMicWs = 12;
constexpr uint8_t kPinI2sMicSd = 14; /* SD：麦克风输出 → 主控输入 */

/*
 * ------------------------------------------------------------
 * RGB LED（PINOUT §2.4 — 阶段 06 接入）
 * ------------------------------------------------------------
 */
constexpr uint8_t kPinRgbDin = 15;
constexpr uint8_t kPinRgbPowerCtrl = 21;

/*
 * ------------------------------------------------------------
 * 扩展 I2C（PINOUT §2.8 — 阶段 02 之后）
 * ------------------------------------------------------------
 */
constexpr uint8_t kPinExtScl = 47;
constexpr uint8_t kPinExtSda = 48;
constexpr uint8_t kPinExtInt = 45;

#endif // EKEYS_HARDWARE_PIN_MAP_H
