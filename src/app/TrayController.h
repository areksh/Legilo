#pragma once

#include "app/Speaker.h"
#include "core/playback/PlaybackController.h"

#include <QObject>

class QAction;
class QMenu;
class QSystemTrayIcon;

namespace legilo {

// Legilo's tray icon (design 7.7, FR-10): its menu carries out the speak
// actions, its icon and tooltip follow the playback state, a middle click
// pauses or resumes, and the speaker's notifications are shown from it.
class TrayController : public QObject
{
    Q_OBJECT

public:
    // The speaker and controller must outlive the tray.
    TrayController(Speaker *speaker, PlaybackController *controller, QObject *parent = nullptr);
    ~TrayController() override;

    void show();
    // Shows a notification from the tray icon.
    void showNotice(Speaker::Notice notice, const QString &title, const QString &message);

signals:
    void quitRequested();

private:
    void showState(PlaybackController::State state);

    Speaker *m_speaker;
    QSystemTrayIcon *m_icon;
    QMenu *m_menu;
    QAction *m_pause;
    QAction *m_stop;
};

} // namespace legilo
