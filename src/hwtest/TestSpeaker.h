/*
 * TestSpeaker.h
 *
 * T5 喇叭测试：
 *
 *   - driver/i2s.h 直接配 I2S_NUM_1 TX：BCLK=IO10 / LRCLK=IO9 / DIN=IO11
 *   - 16bit 立体声（双声道同数据），生成正弦波缓冲循环 i2s_write
 *   - 音调序列：440Hz → 1kHz → 2kHz → 4kHz（各 1s）
 *   - 音量阶梯：1kHz 下幅度 25% → 50% → 75% → 100%
 *   - SW 双击（ROT_ESC）提前结束
 */

#ifndef EKEYS_HWTEST_TEST_SPEAKER_H
#define EKEYS_HWTEST_TEST_SPEAKER_H

#include "HwTestMenu.h"

namespace ekeys
{
    namespace hwtest
    {

        TestBase *createTestSpeaker();

    } // namespace hwtest
} // namespace ekeys

#endif // EKEYS_HWTEST_TEST_SPEAKER_H