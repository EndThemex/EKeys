/*
 * ClickHighlight.cpp
 *
 * 见 ClickHighlight.h。
 */

#include "ClickHighlight.h"

#include "rgb/RGBLightControl.h"

namespace ekeys
{

    ClickHighlight::ClickMode ClickHighlight::mode_ =
        ClickHighlight::CLICK_NONE_COLOR_MODE;

    void ClickHighlight::applySettings(const DeviceSettings &snap)
    {
        mode_ = static_cast<ClickMode>(snap.rgb_click_mode);
    }

    void ClickHighlight::onKeyEdge(uint8_t key_id, bool pressed)
    {
        if (mode_ == CLICK_NONE_COLOR_MODE)
        {
            return;
        }
        if (key_id < 1 || key_id > RGBDriver::kLedCount)
        {
            return;
        }

        const uint8_t led = static_cast<uint8_t>(key_id - 1);
        RGBLightControl::instance().setHighlight(led, pressed);
    }

} // namespace ekeys
