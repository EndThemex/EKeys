#include "BleKeyMap.h"

namespace ekeys
{

    /* ---- 4 套预置 keymap profile ----
     *
     * 命名原则：
     *   - "Numpad"   ：标准数字行（与历史行为一致）
     *   - "Media"    ：F13-F16 + 媒体键（演示/PPT 切换幻灯片）
     *   - "Editor"   ：Ctrl+C / Ctrl+V 等常用编辑器组合 modifier
     *   - "Present"  ：演示模式（Esc 退出，Tab 切换，左右方向键）
     *
     * 旋钮行为（单击/双击/长按/CW/CCW）尽量在所有 profile 都给一个
     * "有意义的默认值"，避免用户切换后旋钮完全无响应。
     *
     * 注意：keyMap / encoderMap / rotateMap 存的是"已经转换好的"库 key 码
     *（直接给 BleKeyboard::press() 用），不是 HID Usage ID。这样 BleKeyboardSink
     * 不用每次都做 hidToLibKey 转换。
     */

    /* helper: 用 HID Usage ID 构造一行的常量 */
    static constexpr uint8_t H_(uint8_t hid)
    {
        return hidToLibKey(hid);
    }

    /* Profile 0: Numpad — 1..9 顶部数字行（默认，与历史行为一致） */
    static const KeyMapProfile kProfileNumpad = {
        "Numpad",
        {
            0,                 // index 0 占位
            H_(0x1E), H_(0x1F), H_(0x20), // 1 2 3
            H_(0x21), H_(0x22), H_(0x23), // 4 5 6
            H_(0x24), H_(0x25), H_(0x26), // 7 8 9
        },
        {
            0,
            H_(0x28), // 单击 → Enter
            H_(0x29), // 双击 → Esc
            H_(0x2B), // 长按 → Tab
        },
        {
            0,
            H_(0x4F), // CW → Right
            H_(0x50), // CCW → Left
        },
    };

    /* Profile 1: Media — 媒体键 + 演示常用
     *
     * 0xCD = Play/Pause, 0xE9 = Volume+, 0xEA = Volume-,
     * 0xB5 = Scan Next Track, 0xB6 = Scan Prev Track,
     * 0x4A = Home, 0x4D = End, 0x4B = PageUp, 0x4E = PageDown
     */
    static const KeyMapProfile kProfileMedia = {
        "Media",
        {
            0,
            H_(0xCD), H_(0xE9), H_(0xEA),       // 1 Play,  2 Vol+, 3 Vol-
            H_(0xB5), H_(0xB6), H_(0x29),       // 4 Next,  5 Prev,  6 Esc
            H_(0x4A), H_(0x4B), H_(0x4E),       // 7 Home,  8 PgUp, 9 PgDn
        },
        {
            0,
            H_(0xCD), // 单击 → Play/Pause
            H_(0x29), // 双击 → Esc
            H_(0x2B), // 长按 → Tab
        },
        {
            0,
            H_(0x4F), // CW → Right（下一首/前进）
            H_(0x50), // CCW → Left（上一首/后退）
        },
    };

    /* Profile 2: Editor — 编辑器常用组合
     *
     * 在 ESP32 BLE Keyboard 库里，按组合键需要 press(mod) + press(key) +
     * releaseAll()（或 release(mod)+release(key)）。
     * 这里只能配"单个 key 码"，所以本 profile 给单键快捷：
     *   - Copy/Paste/Cut/Undo/Redo/Select All/Save/Find/Run
     *   - 用 ASCII 字符 ('c','v','x','z','y','a','s','f','r') 走库码 0..127，
     *     库会自己查 _asciimap 自动应用 shift 状态。
     *
     * 注意：库码 < 0x80 是 ASCII。比如 'C' (大写) 在 _asciimap[0x43] 中
     * 会自动带 LShift。这里我们传字符字面量即可（库码 = ASCII）。
     */
    static const KeyMapProfile kProfileEditor = {
        "Editor",
        {
            0,
            (uint8_t)'c', (uint8_t)'v', (uint8_t)'x', // 1 Copy, 2 Paste, 3 Cut
            (uint8_t)'z', (uint8_t)'a', (uint8_t)'s', // 4 Undo, 5 SelAll, 6 Save
            (uint8_t)'f', (uint8_t)'r', H_(0x2C),    // 7 Find, 8 Run (compile), 9 Space
        },
        {
            0,
            H_(0x28), // 单击 → Enter
            H_(0x29), // 双击 → Esc
            H_(0x2B), // 长按 → Tab
        },
        {
            0,
            H_(0x4F), // CW → Right（光标右移）
            H_(0x50), // CCW → Left（光标左移）
        },
    };

    /* Profile 3: Present — 演示模式
     *
     * 0x06 = 'c' (c), 0x07 = 'd', 0x09 = 'f'
     * 这里用 HID 0x06 = 'c' 给空白键（演示时打 c 翻黑屏，但更常见是 . 或 B
     * 这种应用自定义键，这里给箭头 + Esc 就够用了）。
     */
    static const KeyMapProfile kProfilePresent = {
        "Present",
        {
            0,
            H_(0x06), H_(0x07), H_(0x08),       // 1 'c', 2 'd', 3 'e'
            H_(0x09), H_(0x0A), H_(0x0B),       // 4 'f', 5 'g', 6 'h'
            H_(0x0C), H_(0x0D), H_(0x0E),       // 7 'i', 8 'j', 9 'k'
        },
        {
            0,
            H_(0x28), // 单击 → Enter（幻灯片确认）
            H_(0x29), // 双击 → Esc（退出幻灯片）
            H_(0x2B), // 长按 → Tab
        },
        {
            0,
            H_(0x4F), // CW → Right（下一页）
            H_(0x50), // CCW → Left（上一页）
        },
    };

    /* profile 表 */
    static const KeyMapProfile *const kProfiles[BLE_PROFILE_COUNT] = {
        &kProfileNumpad,
        &kProfileMedia,
        &kProfileEditor,
        &kProfilePresent,
    };

    /* 运行时当前选中的 profile 索引。初始为 0 (Numpad)，main.cpp 在
     * setup() 末尾从 NVS 读出来覆盖。 */
    static uint8_t g_activeProfile = 0;

    uint8_t bleActiveProfile() { return g_activeProfile; }

    const KeyMapProfile &bleProfile(uint8_t idx)
    {
        if (idx >= BLE_PROFILE_COUNT)
            idx = 0;
        return *kProfiles[idx];
    }

    void refreshMapsFromActiveProfile()
    {
        const KeyMapProfile &p = bleProfile(g_activeProfile);
        for (uint8_t i = 0; i < 10; ++i)
            BLE_KEY_MAP[i] = p.keyMap[i];
        for (uint8_t i = 0; i < 4; ++i)
            BLE_ENCODER_MAP[i] = p.encoderMap[i];
        for (uint8_t i = 0; i < 3; ++i)
            BLE_ROTATE_MAP[i] = p.rotateMap[i];
    }

    /* 提供一个 setter 给 main.cpp（启动期一次性从 NVS 恢复后调用） */
    void bleSetActiveProfile(uint8_t idx)
    {
        if (idx >= BLE_PROFILE_COUNT)
            idx = 0;
        g_activeProfile = idx;
        refreshMapsFromActiveProfile();
    }

} // namespace ekeys