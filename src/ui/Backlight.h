#pragma once
#include <Arduino.h>

namespace ekeys
{

  /* 背光控制 —— 单例，全局共享。
   *
   * 用 ESP32 LEDC（PWM）控制 TFT_BL 引脚：
   *   - 频率 5kHz（避开可听频段、避免屏幕横纹）
   *   - 分辨率 8 bit（0~255，128 步可调够用）
   *   - 默认最大占空比（屏幕最亮）
   *
   * 防过度调暗设计：
   *   - 背光最低有效亮度 MIN_EFFECTIVE_PCT = 25%
   *     （PWM 占空比 <25% 时 IPS 屏幕会出现"画面发黑不可读"，
   *      反而失去背光意义。低于此值时调用方应 clamp 到 25%。）
   *   - 持久化到 NVS，下次开机恢复用户上次设置
   *
   * 注意：本项目 BL 引脚 active-LOW（LOW 亮 / HIGH 灭），
   * 见 main.cpp setup() 注释。所以：
   *   - 用户设 brightness 越大 → LCD 越亮
   *   - 设最大 100% → 输出 LEDC duty = 0 → digitalWrite LOW → 最亮
   *   - 设最小 25%  → 输出 LEDC duty ≈ 192（255*75%） → digitalWrite HIGH 一部分时间
   */
  class Backlight
  {
  public:
    /* 背光占空比上下限（百分比，避免过度调暗） */
    static constexpr uint8_t MIN_PCT = 25;  /* 25%：低于此 IPS 屏画面发黑 */
    static constexpr uint8_t MAX_PCT = 100; /* 100%：最亮 */
    static constexpr uint8_t DEFAULT_PCT = MAX_PCT;

    /* PWM 参数 */
    static constexpr uint32_t PWM_FREQ_HZ = 5000;
    static constexpr uint8_t PWM_RES_BITS = 8;                          /* 8 bit → 256 步 */
    static constexpr uint32_t PWM_MAX_DUTY = (1U << PWM_RES_BITS) - 1U; /* 255 */

    static Backlight &instance();

    /* 初始化 LEDC 通道。必须在 setup() 里 gfx->begin() 之后调用。
     * 内部会从 NVS 读取上次的亮度（无则用默认）。 */
    void begin();

    /* 设置当前亮度（百分比）。低于 MIN_PCT 会被 clamp。 */
    void setBrightnessPct(uint8_t pct);

    /* 当前亮度（百分比）。 */
    uint8_t brightnessPct() const { return brightnessPct_; }

  private:
    Backlight() = default;
    Backlight(const Backlight &) = delete;
    Backlight &operator=(const Backlight &) = delete;

    /* 把 pct 转换成 active-LOW 的 LEDC duty。
     * pct 越大 → duty 越小（BL 引脚 LOW 时间越多 → 越亮）。 */
    uint32_t pctToDuty(uint8_t pct) const;

    /* 持久化当前 brightness 到 NVS */
    void saveToNvs();

    /* 从 NVS 读取上次亮度；失败返回默认 */
    uint8_t loadFromNvs();

    bool initialized_{false};
    uint8_t brightnessPct_{DEFAULT_PCT};
  };

} // namespace ekeys