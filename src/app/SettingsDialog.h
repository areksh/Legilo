#pragma once

#include "core/tts/Voice.h"

#include <QDialog>
#include <QList>

namespace legilo {

class Autostart;
class GlobalShortcuts;
class Settings;

// The settings dialog (design 7.9): General, Voice, Shortcuts and a basic
// Clean-up page. Every change applies at once, as KDE's settings do, so
// the dialog only has a Close button. The settings, shortcuts and
// autostart must outlive it.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog(Settings *settings, GlobalShortcuts *shortcuts, Autostart *autostart,
                   const QList<Voice> &voices, QWidget *parent = nullptr);

private:
    QWidget *generalPage();
    QWidget *voicePage(const QList<Voice> &voices);
    QWidget *shortcutsPage();
    QWidget *cleanupPage();
    QString applyShortcut(const QString &id, const QKeySequence &sequence);

    Settings *m_settings;
    GlobalShortcuts *m_shortcuts;
    Autostart *m_autostart;
};

} // namespace legilo
