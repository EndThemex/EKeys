#include "ui/PageManager.h"
#include <lvgl.h>

/* PNG 转换背景图（428x142 RGB565），定义在 ui/assets/bgv1.c */
LV_IMG_DECLARE(bgv1);

namespace ekeys
{

    PageManager::PageManager() = default;

    void PageManager::begin()
    {
        /* LVGL 屏幕已由 main.cpp 的 lvgl_display_init() 创建好。
         * 这里清空 background、铺设 PNG 背景图，并构建默认主题 */
        lv_obj_t *scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
        /* 背景图与屏幕同尺寸（428x142），top-left 对齐即铺满整屏。
         * 页面 root 保持透明（Page::enter()），即可透出此图 */
        lv_obj_set_style_bg_img_src(scr, &bgv1, LV_PART_MAIN);
    }

    void PageManager::registerPage(Page *p)
    {
        if (p == nullptr)
            return;
        for (auto *q : registry_)
        {
            if (q->id() == p->id())
            {
                Serial.printf("[Page] duplicate page id %u ignored\n", p->id());
                return;
            }
        }
        p->attach(this);
        registry_.push_back(p);
    }

    Page *PageManager::current() const
    {
        return stack_.empty() ? nullptr : stack_.back();
    }

    Page *PageManager::findById(uint8_t pageId) const
    {
        for (auto *q : registry_)
        {
            if (q->id() == pageId)
                return q;
        }
        return nullptr;
    }

    void PageManager::push(uint8_t pageId)
    {
        Page *p = nullptr;
        for (auto *q : registry_)
        {
            if (q->id() == pageId)
            {
                p = q;
                break;
            }
        }
        if (p == nullptr)
        {
            Serial.printf("[Page] push: page id %u not registered\n", pageId);
            return;
        }
        if (!stack_.empty())
            exitTop();
        stack_.push_back(p);
        enterTop();
        Serial.printf("[Page] push id=%u (%s), stack depth=%u\n",
                      p->id(), p->title(), (unsigned)stack_.size());
    }

    void PageManager::pop()
    {
        if (stack_.empty())
            return;
        exitTop();
        stack_.pop_back();
        if (stack_.empty())
        {
            /* 栈空：重启主页，避免空白 */
            Page *home = nullptr;
            for (auto *q : registry_)
            {
                if (q->id() == 1)
                {
                    home = q;
                    break;
                }
            }
            if (home != nullptr)
            {
                stack_.push_back(home);
                enterTop();
            }
        }
        else
        {
            enterTop();
        }
        Serial.printf("[Page] pop, stack depth=%u\n", (unsigned)stack_.size());
    }

    void PageManager::replace(uint8_t pageId)
    {
        if (!stack_.empty())
            exitTop();
        stack_.pop_back();
        push(pageId);
    }

    void PageManager::enterTop()
    {
        if (stack_.empty())
            return;
        Page *p = stack_.back();
        p->enter();
    }

    void PageManager::exitTop()
    {
        if (stack_.empty())
            return;
        Page *p = stack_.back();
        p->exit();
    }

    void PageManager::loopTick()
    {
        /* 各页可以轮询自己，但当前只有 TomatoPage 需要 */
        if (Page *p = current())
        {
            /* 没有专门的 tick 钩子；页面 onEnter/onExit 自己装/拆定时器 */
            (void)p;
        }
    }

    void PageManager::handleKeyPress(uint8_t keyId)
    {
        Page *p = current();
        if (p == nullptr)
            return;

        if (keyId == 1)
        {
            /* KEY1 = 退出（弹栈） */
            pop();
            return;
        }
        if (keyId == 2)
        {
            /* KEY2 = 进入/确认 */
            p->onConfirm();
            return;
        }
        /* KEY3..KEY9 = 当前页可自定义（默认：跳到对应页） */
        p->onSelectKey(keyId);
    }

    void PageManager::handleKeyRelease(uint8_t /*keyId*/)
    {
        /* 默认不做处理；页面有需要可扩展 Page::onKeyRelease */
    }

    void PageManager::handleEncoderRotate(int8_t delta)
    {
        if (Page *p = current())
            p->onEncoder(delta);
    }

    void PageManager::handleEncoderClick()
    {
        /* 旋钮按下 = 进入/确认（与 KEY2 同语义）。
         * 之所以统一路由到 onConfirm() 而不是 onEncoderClick()：
         *   - 此前各页 onConfirm() / onEncoderClick() 大量互相调用（Tomato/Ble/KeyMap），
         *     表明设计上两者本就是同一个动作；
         *   - 旋钮单击也作为"进入/确认"，让用户在脱离矩阵键盘的场景下
         *     仍能完整操作 UI（菜单进入子页、番茄钟启停、BLE 子页进入 KeyMap 等）。
         * 个别页原本用 onEncoderClick() 做差异化动作（如 RgbPage 的 toggle on/off），
         * 后续若仍需保留可通过旋转 Power 模式实现。 */
        if (Page *p = current())
            p->onConfirm();
    }

    void PageManager::printStack() const
    {
        Serial.print("[Page] stack:");
        for (auto *p : stack_)
            Serial.printf(" %u", p->id());
        Serial.println();
    }

} // namespace ekeys