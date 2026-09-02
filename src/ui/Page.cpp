#include "ui/Page.h"
#include "ui/PageManager.h"
#include <stdio.h>

namespace ekeys
{

    Page::Page(uint8_t pageId, const char *title, lv_color_t accent)
        : pageId_(pageId), title_(title), accent_(accent) {}

    void Page::enter()
    {
        if (active_)
            return;
        active_ = true;
        /* 在当前活动屏幕创建 root obj。LVGL 同一时刻只有一个"活动屏幕对象树"，所以
         * 删除上一个页面时只需要 del 它的 root，整个子对象树随之删除。 */
        root_ = lv_obj_create(lv_scr_act());
        lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(root_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(root_, 0, LV_PART_MAIN);

        buildUi();
        onEnter();
    }

    void Page::exit()
    {
        if (!active_)
            return;
        active_ = false;
        onExit();
        teardownUi();
        if (root_ != nullptr)
        {
            lv_obj_del(root_);
            root_ = nullptr;
        }
    }

    void Page::requestBack()
    {
        if (mgr_ != nullptr)
            mgr_->pop();
    }

    void Page::requestPush(uint8_t pageId)
    {
        if (mgr_ != nullptr)
            mgr_->push(pageId);
    }

    /* 基类默认按 kind() 路由 KEY3..KEY9 到对应的 selectXxx(idx)。
     *
     * idx = keyId - KEY3_OFFSET（KEY3→0, KEY4→1, ... KEY9→6）。
     * 子类只需在对应类型的 selectXxx() 里返回 true / false 表达"是否消费"，
     * 实际"第 idx 个是什么"的语义由子类自行定义（详见 docs/10-input-mapping-rule.md）。
     *
     * KEY1（back）与 KEY2（confirm）由 PageManager 硬编码处理，不会进入这里。
     * 超出实际数量时，子类 selectXxx 返回 false，基类直接丢弃，绝不回卷。 */
    void Page::onSelectKey(uint8_t keyId)
    {
        constexpr uint8_t KEY3_OFFSET = 3;
        if (keyId < KEY3_OFFSET)
            return;
        const uint8_t idx = (uint8_t)(keyId - KEY3_OFFSET);

        switch (kind())
        {
        case PageKind::List:
            (void)selectItem(idx);
            break;
        case PageKind::Mode:
            (void)selectMode(idx);
            break;
        case PageKind::State:
            (void)selectState(idx);
            break;
        case PageKind::Action:
            (void)selectAction(idx);
            break;
        case PageKind::ReadOnly:
        default:
            /* R 类型：忽略所有 KEY3..KEY9 */
            break;
        }
    }

    /* 按 PageKind 生成 hint 文案（docs/10-input-mapping-rule.md §5）。
     * 模板稳定，状态/数值变化不应让 hint 改变（应在主信息区反馈）。
     * out 由调用者提供，至少 64 字节；超出长度自动截断。 */
    void buildHintLabel(PageKind kind, char *out, size_t outLen)
    {
        if (out == nullptr || outLen == 0)
            return;
        const char *tpl = nullptr;
        switch (kind)
        {
        case PageKind::List:
            tpl = "K1 back  KNOB pick  K2 enter  K3..K9 jump";
            break;
        case PageKind::Mode:
            tpl = "K1 back  KNOB value  K2 next mode  K3 mode";
            break;
        case PageKind::State:
            tpl = "K1 back  KNOB adjust  K2 step  K3..K9 state";
            break;
        case PageKind::Action:
            tpl = "K1 back  KNOB fine  K2 action  K3..K9 action";
            break;
        case PageKind::ReadOnly:
        default:
            tpl = "K1 back";
            break;
        }
        snprintf(out, outLen, "%s", tpl);
    }

} // namespace ekeys