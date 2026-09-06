/*
 * HwTestGlobals.h
 *
 * 测试程序共享入口（main_hwtest.cpp 中定义，其他 Test*.cpp 引用）。
 * 把 g_matrix / g_rotary 等从匿名命名空间暴露到 ekeys::hwtest 命名空间，
 * 避免各 Test 文件自行 extern 匿名 static 变量。
 */

#ifndef EKEYS_HWTEST_HW_TEST_GLOBALS_H
#define EKEYS_HWTEST_HW_TEST_GLOBALS_H

#include "input/MatrixScanner.h"

namespace ekeys
{
    namespace hwtest
    {

        MatrixScanner *getMatrixScanner();

    } // namespace hwtest
} // namespace ekeys

#endif // EKEYS_HWTEST_HW_TEST_GLOBALS_H