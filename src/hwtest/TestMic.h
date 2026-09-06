/*
 * TestMic.h
 *
 * T4 麦克风测试：
 *
 *   1. Mic::begin()（I2S0 RX，16kHz/16bit/mono）
 *   2. 实时 VU：每帧 Read 512 样本 → RMS + 峰值 → 屏幕电平条 + dBFS 数字
 *   3. 录音回放：录 3s 到 PSRAM（96KB）→ Mic::end()（释放 I2S0 与 IO10）
 *      → I2S1 播放刚才的录音
 *   4. SW 双击退出，Mic::end() 兜底
 */

#ifndef EKEYS_HWTEST_TEST_MIC_H
#define EKEYS_HWTEST_TEST_MIC_H

#include "HwTestMenu.h"

namespace ekeys
{
    namespace hwtest
    {

        TestBase *createTestMic();

    } // namespace hwtest
} // namespace ekeys

#endif // EKEYS_HWTEST_TEST_MIC_H