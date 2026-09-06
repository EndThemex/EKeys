/*
 * TestPerf.h
 *
 * T2 屏幕刷新测试：
 *
 *   1. fillScreen 连续 100 帧（红蓝交替）计时
 *   2. 全屏 1/8 区域 fillRect 300 帧
 *   3. draw16bitRGBBitmap 整屏位图搬运 100 帧
 *
 *   - SW 单击（ROT_ENTER）：手动跑下一阶段
 *   - SW 双击（ROT_ESC）：返回菜单
 *
 *   视窗 rotation=1 → 428×142，每帧 ≈ 60816 像素
 */

#ifndef EKEYS_HWTEST_TEST_PERF_H
#define EKEYS_HWTEST_TEST_PERF_H

#include "HwTestMenu.h"

namespace ekeys
{
    namespace hwtest
    {

        TestBase *createTestPerf();

    } // namespace hwtest
} // namespace ekeys

#endif // EKEYS_HWTEST_TEST_PERF_H