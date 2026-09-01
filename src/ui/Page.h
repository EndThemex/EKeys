#pragma once
#include <Arduino.h>
#include <lvgl.h>

namespace ekeys
{

    /* 前置声明，避免头文件互相 include */
    struct PageManager;

    /* 页面基类：所有页面继承此类，并提供 onEnter/onExit/onEncoder/onKey 钩子。
     *
     * 设计动机：
     *   - 每个页面维护自己的 LVGL 对象树（root_ 指向 lv_obj）。
     *   - 进入页面时调用 onEnter()，可重置 UI / 启动定时逻辑；
     *   - 退出页面时调用 onExit()，可关闭定时器 / 释放资源；
     *   - 旋钮 / 旋钮单击 / KEY2 事件由 PageManager 路由到当前页的同名钩子；
     *   - 页面可以通过 requestBack() 请求弹栈（处理 KEY1）。
     */
    class Page
    {
    public:
        /* pageId 必须全局唯一；推荐用枚举/类内静态常量 */
        Page(uint8_t pageId, const char *title, lv_color_t accent);
        virtual ~Page() = default;

        /* PageManager 会调用这些钩子 */
        void attach(PageManager *mgr) { mgr_ = mgr; }

        /* 生命周期：由 PageManager 调用，不允许重写 */
        void enter();
        void exit();

        /* 事件钩子：默认空实现，页面按需重写
         *
         * onEncoder(delta): 旋钮旋转，delta = ±1（每步）
         * onConfirm():      "进入/确认"动作 —— 触发源有两个：
         *                    - KEY2 矩阵键按下
         *                    - 旋钮按下（旋钮按键）
         *                    统一抽象成"用户希望确认/进入"，避免页面写两份等价代码。
         * onSelectKey(keyId): 其他矩阵键（KEY3..KEY9）按下，用于快速跳页等
         */
        virtual void onEncoder(int8_t /*delta*/) {}
        virtual void onConfirm() {}
        virtual void onSelectKey(uint8_t /*keyId*/) {}

        /* 旋钮旋转是否被本页面"消费"（用于页面参数调整等）
         *
         * true  (默认): 旋转只调 onEncoder(delta)，不会同时发 BLE 方向键。
         *                适用于 RgbPage（调亮度/灯效）、TomatoPage（调时长）等
         *                "旋钮用于调参数"的页面，避免误触发 Host 浏览器前进/后退。
         * false:        旋转穿透到 BLE 方向键发送，同时调 onEncoder(delta)。
         *                适用于 MenuPage（切菜单项本身是 UI 动作，方向键是 Host 输入）
         *                和纯展示页（Status/BLE）。
         */
        virtual bool consumesEncoder() const { return true; }

        uint8_t id() const { return pageId_; }
        const char *title() const { return title_; }
        lv_obj_t *root() const { return root_; }
        bool isActive() const { return active_; }

    protected:
        /* 子类使用的工具 */
        void requestBack();               // 请求 PageManager 弹栈
        void requestPush(uint8_t pageId); // 请求入栈指定页

        /* 子类必须实现的 UI 构建/销毁 */
        virtual void buildUi() = 0;
        virtual void teardownUi() {}

        /* 子类可选重写的进入/退出钩子（区别于 attach-级别的 enter/exit） */
        virtual void onEnter() {}
        virtual void onExit() {}

    private:
        uint8_t pageId_;
        const char *title_;
        lv_color_t accent_;
        lv_obj_t *root_{nullptr};
        PageManager *mgr_{nullptr};
        bool active_{false};
    };

} // namespace ekeys