#pragma once
#include <Arduino.h>

/* 与 main.cpp 的 SCREEN_WIDTH / SCREEN_HEIGHT 同步。
 * 放在 ui 子系统的"全局头"，避免 main.cpp 引用 lvgl。 */
#ifndef SCREEN_W_PX
#define SCREEN_W_PX 428
#endif
#ifndef SCREEN_H_PX
#define SCREEN_H_PX 142
#endif

namespace ekeys
{

    /* 全局页面 id。注册时由各 Page 子类的构造函数传入。 */
    enum PageId : uint8_t
    {
        PAGE_MENU = 1,   // 主菜单（首页）
        PAGE_RGB = 2,    // RGB 控制
        PAGE_TOMATO = 3, // 番茄钟
        PAGE_STATUS = 4, // 系统状态（保留）
        PAGE_BLE = 5,    // 蓝牙连接状态
    };

} // namespace ekeys