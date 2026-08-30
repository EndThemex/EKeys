#pragma once
#include "KeyEvent.h"

namespace ekeys {

class IKeySource {
public:
    virtual ~IKeySource() = default;
    virtual void begin() = 0;
    virtual void poll(KeyEventList& out) = 0;
};

}  // namespace ekeys
