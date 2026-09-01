#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <vector>
#include "ui/Page.h"

namespace ekeys
{

  /* 页面管理器
   *
   *   - 全局单例（g_pageManager）。
   *   - 维护页面注册表 + 当前栈。
   *   - 负责 LVGL 屏幕生命周期、页面创建/销毁、事件路由。
   *
   * 用法：
   *   g_pageManager.begin(gfx);                   // 在 setup() 里调用一次
   *   g_pageManager.registerPage(&status_page);    // 注册（顺序任意）
   *   g_pageManager.registerPage(&rgb_page);
   *   g_pageManager.registerPage(&tomato_page);
   *   g_pageManager.push(startPageId);             // 入栈首页
   *
   * 事件路由（来自 main loop，从队列取出事件后调用）：
   *   g_pageManager.handleKeyPress(keyId);
   *   g_pageManager.handleEncoderRotate(delta);
   *   g_pageManager.handleEncoderClick();
   *
   * 按键语义：
   *   - KEY1 -> 弹栈（回上一级页面）
   *   - KEY2 -> 进入/确认（路由到当前页 onConfirm()）
   *   - KEY3..KEY9 -> 路由到当前页 onSelectKey(keyId)
   *
   * 旋钮语义：
   *   - 旋转 -> onEncoder(delta)
   *   - 按下 -> onConfirm()（与 KEY2 同义，统一为"进入/确认"）
   *
   * 页面自身决定 KEY2 / 旋钮按下 在自己的页面里到底是什么动作。
   */
  class PageManager
  {
  public:
    PageManager();

    void begin();
    void loopTick(); // 每帧调用：推进番茄钟等需要 frame tick 的逻辑

    /* 注册/入栈/弹栈 */
    void registerPage(Page *p);
    void push(uint8_t pageId);
    void pop();
    void replace(uint8_t pageId); // 直接切换（不增加栈深度）

    Page *current() const;
    Page *findById(uint8_t pageId) const; // 在 registry 里查找（不触发 enter/exit）

    /* 事件路由（从 main loop 调用） */
    void handleKeyPress(uint8_t keyId);
    void handleKeyRelease(uint8_t keyId);
    void handleEncoderRotate(int8_t delta);
    void handleEncoderClick();

    /* 调试 */
    void printStack() const;

  private:
    void enterTop();
    void exitTop();

    std::vector<Page *> registry_;
    std::vector<Page *> stack_;
  };

} // namespace ekeys