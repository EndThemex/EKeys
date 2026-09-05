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
 * I2S 音频（PINOUT §2.7 — 阶段 06 接入）
 *
 * 功放（MAX98357）：BCLK=IO10 / LRCLK=IO9 / SDOUT=IO14（I2S1 TX）
 * 麦克风（ICS43434）：BCLK=IO10 / SCK=IO13 / WS=IO12 / SD=IO11（I2S0 RX）
 *
 * 注意：麦克风 BCLK（IO10）与功放 BCLK 共用同一 GPIO；
 * I2S0 / I2S1 各自为主模式输出时钟时会双驱 IO10，
 * 因此录音前必须先停 Speaker、播放前必须先停录音，
 * 互斥由 voice::VoiceRecognizer 与 audio::Speaker 互相检查保证。
 * ------------------------------------------------------------
 */
constexpr uint8_t kPinI2sBclkSpeaker = 10;
constexpr uint8_t kPinI2sLrclkSpeaker = 9;
constexpr uint8_t kPinI2sDataSpeaker = 14;
constexpr uint8_t kPinI2sMicBclk = 10;     // 与 Speaker BCLK 共用，运行时互斥
constexpr uint8_t kPinI2sMicWs = 12;
constexpr uint8_t kPinI2sMicSck = 13;
constexpr uint8_t kPinI2sMicDin = 11;

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
