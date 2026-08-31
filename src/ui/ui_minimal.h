/*
 * ui_minimal.h
 *
 * 阶段 02 期间最小主屏：标题 + 时间标签。
 * 阶段 05 由 SquareLine Studio 生成的 src/ui/ui.cpp 接管。
 */

#ifndef EKEYS_UI_UI_MINIMAL_H
#define EKEYS_UI_UI_MINIMAL_H

namespace ekeys {

class ui_minimal {
public:
    /*
     * 初次创建主屏（在 DisplayTask 启动之前调用一次）。
     */
    static void create();

    /*
     * 由 DisplayTask 在收到 DisplayMessageType::TimeUpdate 时调用。
     * text 应为 "HH:MM:SS" 形式的 NUL 结尾字符串。
     */
    static void setTimeLabel(const char *text);
};

}  // namespace ekeys

#endif  // EKEYS_UI_UI_MINIMAL_H
