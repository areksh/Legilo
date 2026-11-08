#pragma once

#include <QKeySequence>
#include <QObject>
#include <QString>

namespace legilo {

// System-wide keyboard shortcuts for Legilo's actions (design 7.6). The
// actions and their defaults are in 7.6.2; each platform keeps the
// shortcuts its own way, and on KDE they can also be changed in System
// Settings, which this interface reports through shortcutChanged.
class GlobalShortcuts : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // Registers an action with its default, which applies only if the user
    // has not chosen a shortcut and the default is not taken. An empty
    // default leaves the action without a shortcut.
    virtual void registerAction(const QString &id, const QString &label,
                                const QKeySequence &defaultSequence) = 0;
    // Assigns a shortcut, or clears it with an empty sequence. Returns
    // false if it could not be assigned, for example because it is taken.
    virtual bool setShortcut(const QString &id, const QKeySequence &sequence) = 0;
    // The action's shortcut, empty if it has none.
    virtual QKeySequence shortcut(const QString &id) const = 0;
    // Whether no other application holds the sequence.
    virtual bool isAvailable(const QKeySequence &sequence) const = 0;

signals:
    void activated(const QString &id);
    void shortcutChanged(const QString &id, const QKeySequence &sequence);
};

} // namespace legilo
