/*
 * TestKeys.h
 *
 * T3 矩阵按键测试：
 *
 *   - 屏幕画 3 行 × 4 列键位网格（ROW0-COL3 空位画 "×"）
 *   - 按下高亮 + 计数 + 串口日志
 *   - 同时点亮对应 WS2812（下标 = keyId - 1），与 RGB 联动互验
 *   - SW 双击（ROT_ESC）：返回菜单
 */

#ifndef EKEYS_HWTEST_TEST_KEYS_H
#define EKEYS_HWTEST_TEST_KEYS_H

#include "HwTestMenu.h"

namespace ekeys
{
    namespace hwtest
    {

        TestBase *createTestKeys();

    } // namespace hwtest
} // namespace ekeys

#endif // EKEYS_HWTEST_TEST_KEYS_H