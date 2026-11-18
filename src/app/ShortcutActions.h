#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>

namespace legilo {

// A global shortcut action (design 7.6.2): its ID, which the platform
// stores the shortcut under and which matches an IPC command, its label
// for the settings dialog and System Settings, and its Linux default.
struct ShortcutAction
{
    QString id;
    QString label;
    QKeySequence defaultSequence; // empty for none
};

// The actions, in the order the Shortcuts page lists them. Show-reader
// joins them with the reader, in stage 6.
QList<ShortcutAction> shortcutActions();

// Why a sequence cannot be a global shortcut, or an empty string if it
// can (design 7.6.5): it must be a single chord with Ctrl, Alt or Meta,
// because Shift alone would take a key away from typing. An empty
// sequence is fine: it clears the shortcut.
QString shortcutProblem(const QKeySequence &sequence);

} // namespace legilo
