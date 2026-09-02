#pragma once
#include <Arduino.h>
#include <lvgl.h>

namespace ekeys
{

    /* 前置声明，避免头文件互相 include */
    struct PageManager;

    /* 页面类型分类 —— 决定 KEY3..KEY9 与旋钮的默认语义。
     * 见 docs/10-input-mapping-rule.md §3。 */
    enum class PageKind : uint8_t
    {
        List = 0,    // L：列表选择（Menu、KeyMap）
        Mode = 1,    // M：多模式控制（RGB）
        State = 2,   // S：流程状态机（Tomato）
        Action = 3,  // A：即时动作（BLE）
        ReadOnly = 4,// R：只读展示（Mic / Status）
    };

    /* 页面基类：所有页面继承此类，并提供 onEnter/onExit/onEncoder/onKey 钩子。
     *
     * 设计动机：
     *   - 每个页面维护自己的 LVGL 对象树（root_ 指向 lv_obj）。
     *   - 进入页面时调用 onEnter()，可重置 UI / 启动定时逻辑；
     *   - 退出页面时调用 onExit()，可关闭定时器 / 释放资源；
     *   - 旋钮 / 旋钮单击 / KEY2 事件由 PageManager 路由到当前页的同名钩子；
     *   - 页面可以通过 requestBack() 请求弹栈（处理 KEY1）。
     *
     * PageKind 路由：
     *   - 子类声明 kind()，基类按 kind() 自动把 KEY3..KEY9 路由到
     *     selectItem/selectMode/selectState/selectAction(idx)。
     *   - 子类只需要"声明第 N 个是什么"，不必再写 if-else。
     *   - 超出实际数量的 KEY 一律无操作，绝不回卷。
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
         * onSelectKey(keyId): 其他矩阵键（KEY3..KEY9）按下。
         *                    基类默认按 kind() 路由到 selectXxx(idx)；
         *                    子类一般不需要重写本函数。
         */
        virtual void onEncoder(int8_t /*delta*/) {}
        virtual void onConfirm() {}
        virtual void onSelectKey(uint8_t keyId);

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

        /* 子类必须声明自己属于哪一类，决定 KEY3..KEY9 / 旋钮的默认语义。
         * 详见 docs/10-input-mapping-rule.md §3。 */
        virtual PageKind kind() const = 0;

        /* 类型相关钩子（基类按 kind() 默认路由，子类按需重写其中之一）：
         *
         *   - selectItem(idx):   L 类型，KEY3..KEY9 → 直接跳到第 idx 项（idx 从 0 开始）
         *   - selectMode(idx):   M 类型，KEY3..KEY9 → 直接进入第 idx 个模式
         *   - selectState(idx):  S 类型，KEY3..KEY9 → 直接进入第 idx 个状态
         *   - selectAction(idx): A 类型，KEY3..KEY9 → 直接触发第 idx 个动作
         *
         * 默认实现均为 no-op；子类应只重写自己 kind() 对应的那一个。
         * "第 idx 个"的具体含义（如"idx=0 = Effect / IDLE / toggle"）由子类文档/hint 说明。
         *
         * 返回 true 表示已消费（基类不再穿透），false 表示未处理。
         * 实际数量上限由子类各自保证；超出 idx 范围时返回 false 即可，基类不会回卷。 */
        virtual bool selectItem(uint8_t /*idx*/)  { return false; }
        virtual bool selectMode(uint8_t /*idx*/)  { return false; }
        virtual bool selectState(uint8_t /*idx*/) { return false; }
        virtual bool selectAction(uint8_t /*idx*/) { return false; }

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

    /* 按 PageKind 生成 hint 文案模板（docs/10-input-mapping-rule.md §5）。
     * out 至少 64 字节。
     * 写入以 '\0' 结尾的 C 字符串。 */
    void buildHintLabel(PageKind kind, char *out, size_t outLen);

} // namespace ekeys