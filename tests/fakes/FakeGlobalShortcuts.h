#pragma once

#include "platform/GlobalShortcuts.h"

#include <QHash>
#include <QSet>

namespace legilo::testing {

// GlobalShortcuts kept in memory. Sequences in taken belong to another
// application; changeElsewhere() is a change made in System Settings.
class FakeGlobalShortcuts : public GlobalShortcuts
{
public:
    QSet<QKeySequence> taken;

    void registerAction(const QString &id, const QString &label,
                        const QKeySequence &defaultSequence) override
    {
        Q_UNUSED(label);
        m_shortcuts.insert(id, taken.contains(defaultSequence) ? QKeySequence() : defaultSequence);
    }

    bool setShortcut(const QString &id, const QKeySequence &sequence) override
    {
        if (!m_shortcuts.contains(id) || taken.contains(sequence)) {
            return false;
        }
        m_shortcuts.insert(id, sequence);
        emit shortcutChanged(id, sequence);
        return true;
    }

    QKeySequence shortcut(const QString &id) const override { return m_shortcuts.value(id); }

    bool isAvailable(const QKeySequence &sequence) const override
    {
        return !taken.contains(sequence);
    }

    void changeElsewhere(const QString &id, const QKeySequence &sequence)
    {
        m_shortcuts.insert(id, sequence);
        emit shortcutChanged(id, sequence);
    }

private:
    QHash<QString, QKeySequence> m_shortcuts;
};

} // namespace legilo::testing
