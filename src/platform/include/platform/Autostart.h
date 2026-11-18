#pragma once

namespace legilo {

// Starting Legilo in the tray when the user logs in (design 7.7, FR-12).
// Each platform keeps the choice its own way: an XDG autostart entry on
// Linux, a Run key value on Windows. What the system holds is the truth,
// so a choice changed in System Settings is seen here too.
class Autostart
{
public:
    Autostart() = default;
    Autostart(const Autostart &) = delete;
    Autostart &operator=(const Autostart &) = delete;
    virtual ~Autostart() = default;

    virtual bool isEnabled() const = 0;
    // Returns false if the system's entry could not be changed.
    virtual bool setEnabled(bool enabled) = 0;
};

} // namespace legilo
