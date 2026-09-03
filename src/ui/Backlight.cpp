#include "ui/Backlight.h"
#include <Preferences.h>

/* TFT_BL 在 main.cpp 用 #define 定义（无类型符号）。
 * 头文件里没有声明，调用方直接用 #include "main.cpp" 顶部那块不优雅，
 * 这里把引脚号重复一次 + 加注释提醒：如果改 TFT_BL 必须同步这里。
 * 实际上 TFT_BL 是 37；见 main.cpp:#define TFT_BL 37。 */
#ifndef TFT_BL
#define TFT_BL 37
#endif

namespace ekeys
{

    Backlight &Backlight::instance()
    {
        static Backlight g_backlight;
        return g_backlight;
    }

    uint32_t Backlight::pctToDuty(uint8_t pct) const
    {
        /* active-LOW：pct 越大 → duty 越小（LOW 时间越长 → 越亮）。
         *   pct = 100  → duty = 0   （常低 → 最亮）
         *   pct = 25   → duty = 191 （75% 高电平 → 25% 亮度）
         *   pct = 0    → duty = 255 （常高 → 灭）
         *
         * 这里 pct 已经保证 >= MIN_PCT（=25），duty 范围 [0, 191]。 */
        if (pct > MAX_PCT)
            pct = MAX_PCT;
        /* duty = (100 - pct) * PWM_MAX_DUTY / 100 */
        return (uint32_t)(MAX_PCT - pct) * PWM_MAX_DUTY / MAX_PCT;
    }

    void Backlight::begin()
    {
        if (initialized_)
            return;

        /* 从 NVS 读上次亮度；失败用默认 */
        brightnessPct_ = loadFromNvs();

        /* 配置 PWM 通道。ESP32 Arduino core 2.x/3.x 都支持这套签名。 */
        ledcSetup(/*channel=*/0, PWM_FREQ_HZ, PWM_RES_BITS);
        ledcAttachPin(TFT_BL, /*channel=*/0);

        /* 应用初始亮度 */
        ledcWrite(/*channel=*/0, (int)pctToDuty(brightnessPct_));

        initialized_ = true;
    }

    void Backlight::setBrightnessPct(uint8_t pct)
    {
        /* 防过度调暗：clamp 到有效范围 [MIN_PCT, MAX_PCT] */
        if (pct < MIN_PCT)
            pct = MIN_PCT;
        if (pct > MAX_PCT)
            pct = MAX_PCT;

        if (pct == brightnessPct_)
            return;

        brightnessPct_ = pct;
        if (initialized_)
        {
            ledcWrite(/*channel=*/0, (int)pctToDuty(pct));
        }
        saveToNvs();
    }

    /* NVS 操作集中处理。用一个 file-static Preferences 实例 + 一次性 begin，
     * 与 BLE profile 持久化（main.cpp 的 saveBleProfileToNvs）保持同样的简洁风格。 */
    namespace
    {
        constexpr const char *NVS_NS = "ekeys_bl";
        constexpr const char *NVS_KEY = "pct";
        Preferences g_nvs;
        bool g_nvsReady = false;

        void ensureNvs()
        {
            if (g_nvsReady)
                return;
            g_nvs.begin(NVS_NS, false); /* read-write */
            g_nvsReady = true;
        }
    }

    void Backlight::saveToNvs()
    {
        ensureNvs();
        g_nvs.putUChar(NVS_KEY, brightnessPct_);
    }

    uint8_t Backlight::loadFromNvs()
    {
        /* 用只读模式 begin，避免覆盖已有命名空间。 */
        Preferences nvs;
        if (!nvs.begin(NVS_NS, true))
            return DEFAULT_PCT;
        uint8_t v = nvs.getUChar(NVS_KEY, DEFAULT_PCT);
        nvs.end();
        if (v < MIN_PCT)
            v = MIN_PCT;
        if (v > MAX_PCT)
            v = MAX_PCT;
        return v;
    }

} // namespace ekeys