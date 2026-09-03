#include "ui/AvatarPage.h"
#include "ui/Pages.h"
#include <Arduino.h>

namespace ekeys
{

    /* 顶部状态文字位置：y=4 留 16px 给单行 14pt 字
     * 底部 hint 文字位置：y=124 留 14px 给单行 12pt 字
     * EAvatar 占 y=24..118 */
    static constexpr int16_t STATUS_LABEL_Y = 4;
    static constexpr int16_t HINT_LABEL_Y = 124;

    /* hint 文字模板：保持简短，避免与菜单页模板混淆。
     * 单行居中，可显示在 428px 屏宽内。 */
    static const char *HINT_TEXT = "KEY2 / KNOB  ->  menu";

    AvatarPage::AvatarPage()
        : Page(/*id=*/PAGE_AVATAR, "Avatar", lv_color_hex(0x050505))
    {
    }

    AvatarPage::~AvatarPage()
    {
        /* EAvatar 析构：lv_timer + lv_obj 都在 teardownUi() 里清理，
         * 这里只兜底，确保即使异常退出也不会泄漏 timer。 */
        teardownUi();
    }

    void AvatarPage::buildUi()
    {
        lv_obj_t *root_obj = root();

        /* root 透出 PageManager 的 PNG 背景 */
        lv_obj_set_style_bg_opa(root_obj, LV_OPA_TRANSP, LV_PART_MAIN);

        /* 顶部状态文字 */
        status_label_ = lv_label_create(root_obj);
        lv_label_set_text(status_label_, "EKeys");
        lv_obj_set_style_text_color(status_label_, lv_color_hex(0xA69FAF), LV_PART_MAIN);
        lv_obj_set_style_text_font(status_label_, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(status_label_, LV_ALIGN_TOP_LEFT, 8, STATUS_LABEL_Y);

        /* 底部 hint 文字 */
        hint_label_ = lv_label_create(root_obj);
        lv_label_set_text(hint_label_, HINT_TEXT);
        lv_obj_set_style_text_color(hint_label_, lv_color_hex(0xA69FAF), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint_label_, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(hint_label_, LV_ALIGN_BOTTOM_MID, 0, -4);

        /* 创建 EAvatar widget（占满 root，背景透出） */
        avatar_.begin(root_obj, SCREEN_W_PX, SCREEN_H_PX);
    }

    void AvatarPage::teardownUi()
    {
        /* 必须先停掉 EAvatar 的 30Hz timer：lv_timer 不属于 lv_obj 对象树，
         * 下面 Page::exit() 随后的 lv_obj_del(root_) 不会删除它。若不停，
         * obj_ 变悬空指针后 timerCb → invalidate() 会在 ≤33ms 内访问野指针，
         * 触发 Core 1 LoadProhibited（push Menu 后立即崩溃的根因）。
         * 重新 enter() 时 buildUi() → avatar_.begin() 会重建 obj_ 与 timer_。 */
        avatar_.stop();
        status_label_ = nullptr;
        hint_label_ = nullptr;
    }

    void AvatarPage::onEnter()
    {
        /* 触发 WakeUp：让状态机从零起步。lv_obj 已经建好，30Hz timer 也已启动。 */
        avatar_.event(eavatar::AvatarEvent::WakeUp);
        /* 重置注视到中央 */
        avatar_.setGaze(0.0f, 0.0f);
    }

    void AvatarPage::onExit()
    {
        /* 离开主页：复位 speaking 强度（防止从 Speaking 状态回到主页仍带着张嘴表情） */
        avatar_.setSpeakingLevel(0.0f);
    }

    void AvatarPage::onEncoder(int8_t delta)
    {
        /* 旋钮旋转：把归一化方向映射到 avatar 的 gaze
         *   +1 (顺时针) → x = +0.5（avatar 看向右）
         *   -1 (逆时针) → x = -0.5（看向左）
         * 注视偏移同时由 BLE 路径消费（consumesEncoder() 默认返回 true，
         * 但主页需要把旋转"穿透"到 BLE 方向键，所以下面 override 后让 BLE 收到）。
         * 此处仅调 setGaze；BLE 方向键由 main loop 根据 consumesEncoder() 分发。 */
        static float gazeX = 0.0f;
        gazeX += (float)delta * 0.35f;
        if (gazeX > 1.0f) gazeX = 1.0f;
        if (gazeX < -1.0f) gazeX = -1.0f;
        avatar_.setGaze(gazeX, 0.0f);
    }

    void AvatarPage::onConfirm()
    {
        /* 主页 KEY2 / 旋钮单击 → 进入菜单 */
        requestPush(PAGE_MENU);
    }

    /* 让旋钮旋转穿透到 BLE 方向键（与菜单页一致） */
    /* 注意：默认 Page::consumesEncoder() 返回 true，
     * 这里 override 为 false 让 main loop 同步发 BLE 方向键。 */
    /* AvatarPage 当前选择保持 consumesEncoder() = true（默认），
     * 原因：用户已经在主页盯着头像看旋转时，再触发 BLE 方向键容易让桌面跳页，
     * 体验上不如"avatar 自己看向左/右"自然。
     * 如果以后想穿透 BLE，把下面这一行解除注释即可。 */
    // bool AvatarPage::consumesEncoder() const override { return false; }

} // namespace ekeys
