#pragma once

#include "platform/Autostart.h"

namespace legilo::testing {

// Autostart that remembers its state, and can be made to fail.
class FakeAutostart : public Autostart
{
public:
    bool enabled = false;
    bool fails = false;

    bool isEnabled() const override { return enabled; }

    bool setEnabled(bool value) override
    {
        if (fails) {
            return false;
        }
        enabled = value;
        return true;
    }
};

} // namespace legilo::testing
