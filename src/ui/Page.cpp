#include "ui/Page.h"
#include "ui/PageManager.h"

namespace ekeys {

Page::Page(uint8_t pageId, const char *title, lv_color_t accent)
    : pageId_(pageId), title_(title), accent_(accent) {}

void Page::enter() {
    if (active_) return;
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

void Page::exit() {
    if (!active_) return;
    active_ = false;
    onExit();
    teardownUi();
    if (root_ != nullptr) {
        lv_obj_del(root_);
        root_ = nullptr;
    }
}

void Page::requestBack() {
    if (mgr_ != nullptr) mgr_->pop();
}

void Page::requestPush(uint8_t pageId) {
    if (mgr_ != nullptr) mgr_->push(pageId);
}

}  // namespace ekeys