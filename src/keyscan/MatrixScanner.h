#pragma once
#include "IKeySource.h"
#include "KeyScanConfig.h"

namespace ekeys {

class MatrixScanner : public IKeySource {
public:
    explicit MatrixScanner(const KeyScanConfig& cfg);

    void begin() override;
    void poll(KeyEventList& out) override;

    // 便捷查询
    uint32_t stableMask() const { return stableMask_; }
    bool     isPressed(uint8_t keyId) const;

private:
    enum class State : uint8_t {
        IDLE,
        DEBOUNCE_PRESS,
        PRESSED,
        DEBOUNCE_RELEASE,
    };

    struct DebounceState {
        State     state{State::IDLE};
        uint32_t  enterMs{0};
    };

    void   initGpio();
    void   driveRowLow(uint8_t row);
    void   releaseRow(uint8_t row);
    void   releaseAllRows();
    void   advance(uint8_t keyId, bool rawPressed, uint32_t nowMs, KeyEventList& out);
    void   setStableBit(uint8_t keyId, bool pressed);

    const KeyScanConfig& cfg_;
    DebounceState states_[KEY_NUM]{};
    uint32_t stableMask_{0};
};

}  // namespace ekeys
