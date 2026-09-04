#pragma once
#include "ui/Page.h"
#include "ui/Pages.h"
#include "EAvatar/eavatar_lvgl83.h"
#include <lvgl.h>

namespace ekeys
{

    /* 主页：EAvatar 动画
     *
     * 屏幕 142×428 portrait：
     *   y = 4..20   顶部状态文字 "EKeys" + 电池/BLE 图标位（占位 hint）
     *   y = 24..118 主体：EAvatar blob + 眼睛（约 94 px 高，居中靠上）
     *   y = 124..138 底部 hint "KNOB / KEY2  enter menu"
     *
     * 交互：
     *   - KEY2 按下 → requestPush(PAGE_MENU)，进入菜单
     *   - 旋钮单击 → requestPush(PAGE_MENU)，进入菜单（与 KEY2 同义）
     *   - 旋钮旋转 → 控制 avatar 注视方向 + 同时发 BLE 方向键（不消费）
     *
     * 主页特性：
     *   - 默认状态：Idle + Neutral，持续呼吸 / 眨眼 / 注视微漂
     *   - 进入页面时触发 WakeUp 事件，让动画状态机从零起步
     *   - 不响应 KEY3..KEY9（保持 ReadOnly 性质，避免误触菜单项） */
    class AvatarPage : public Page
    {
    public:
        AvatarPage();
        ~AvatarPage() override;

        void onEnter() override;
        void onExit() override;

        /* 主页是 ReadOnly：不响应 KEY3..KEY9 */
        PageKind kind() const override { return PageKind::ReadOnly; }

        /* 主页旋钮旋转：
         *   - 调 avatar.setGaze() 让头像看向旋转方向
         *   - 不消费 → 同步发 BLE 方向键（让用户在桌面切歌/翻页） */
        void onEncoder(int8_t delta) override;

        /* 主页旋钮单击 → 进入菜单（与 KEY2 同义） */
        void onConfirm() override;

        /* KEY3..KEY9 → 切换 7 个动画状态
         *   KEY3 → Idle
         *   KEY4 → Thinking
         *   KEY5 → Wink
         *   KEY6 → Wide
         *   KEY7 → Alert
         *   KEY8 → Notify
         *   KEY9 → Sleep
         * 虽然 kind()=ReadOnly 会让基类默认忽略，这里显式 override 把按键
         * 接到 avatar_.setState()，让主页从"纯展示"升级为"可触发"。 */
        void onSelectKey(uint8_t keyId) override;

        /* KEY2 按下 → 进入菜单 */
        /* 默认 Page::onSelectKey 已经会落到 kind() 路由；主页不需要响应任何 KEY3..KEY9。 */

        /* 暴露给外部驱动 speaking 强度（MicPage 接入时调用） */
        eavatar::EAvatar &avatar() { return avatar_; }

    private:
        void buildUi() override;
        void teardownUi() override;

        /* EAvatar widget 包装 */
        eavatar::EAvatar avatar_;

        /* 顶部状态文字（占位提示） */
        lv_obj_t *status_label_{nullptr};
        /* 底部 hint 文字 */
        lv_obj_t *hint_label_{nullptr};
    };

} // namespace ekeys
