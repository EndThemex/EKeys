/*
 * TestRotary.h
 *
 * T5 旋钮测试：
 *
 *   - 屏幕中央大号显示累计计数值
 *   - 左转 -1 / 右转 +1
 *   - 方向箭头指示
 *   - SW 单击计数 / SW 双击计数 分行显示
 *   - SW 双击返回菜单
 */

#ifndef EKEYS_HWTEST_TEST_ROTARY_H
#define EKEYS_HWTEST_TEST_ROTARY_H

#include "HwTestMenu.h"

namespace ekeys
{
    namespace hwtest
    {

        TestBase *createTestRotary();

    } // namespace hwtest
} // namespace ekeys

#endif // EKEYS_HWTEST_TEST_ROTARY_H