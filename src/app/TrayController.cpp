#include "app/TrayController.h"

#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

constexpr int NoticeMs = 4000;

// Theme icons until Legilo has its own (stage 3 packaging).
QIcon iconFor(PlaybackController::State state)
{
    switch (state) {
    case PlaybackController::State::Starting:
    case PlaybackController::State::Playing:
        return QIcon::fromTheme(u"media-playback-start"_s);
    case PlaybackController::State::Paused:
        return QIcon::fromTheme(u"media-playback-pause"_s);
    case PlaybackController::State::Idle:
        break;
    }
    return QIcon::fromTheme(u"audio-volume-high"_s);
}

} // namespace

TrayController::TrayController(Speaker *speaker, PlaybackController *controller, QObject *parent)
    : QObject(parent)
    , m_speaker(speaker)
    , m_icon(new QSystemTrayIcon(this))
    , m_menu(new QMenu)
{
    m_menu->addAction(tr("Speak selection"), speaker, &Speaker::speakSelection);
    m_menu->addAction(tr("Speak clipboard"), speaker, &Speaker::speakClipboard);
    m_pause = m_menu->addAction(tr("Pause"), speaker, &Speaker::togglePause);
    m_stop = m_menu->addAction(tr("Stop"), speaker, &Speaker::stop);
    m_menu->addSeparator();
    m_menu->addAction(tr("Quit"), this, &TrayController::quitRequested);
    m_icon->setContextMenu(m_menu);

    connect(m_icon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                // A left click opens the reader from stage 6.
                if (reason == QSystemTrayIcon::MiddleClick) {
                    m_speaker->togglePause();
                }
            });
    connect(controller, &PlaybackController::stateChanged, this, &TrayController::showState);
    connect(speaker, &Speaker::notify, this, &TrayController::showNotice);
    showState(controller->state());
}

TrayController::~TrayController()
{
    // The menu has no parent: QSystemTrayIcon does not take ownership.
    delete m_menu;
}

void TrayController::show()
{
    m_icon->show();
}

void TrayController::showState(PlaybackController::State state)
{
    QString status;
    switch (state) {
    case PlaybackController::State::Idle:
        status = tr("Idle");
        break;
    case PlaybackController::State::Starting:
    case PlaybackController::State::Playing:
        status = tr("Speaking");
        break;
    case PlaybackController::State::Paused:
        status = tr("Paused");
        break;
    }
    m_icon->setIcon(iconFor(state));
    m_icon->setToolTip(tr("Legilo: %1").arg(status));
    m_pause->setText(state == PlaybackController::State::Paused ? tr("Resume") : tr("Pause"));
    m_pause->setEnabled(state != PlaybackController::State::Idle);
    m_stop->setEnabled(state != PlaybackController::State::Idle);
}

void TrayController::showNotice(Speaker::Notice notice, const QString &title,
                                const QString &message)
{
    const auto icon = notice == Speaker::Notice::Problem ? QSystemTrayIcon::Warning
                                                         : QSystemTrayIcon::Information;
    m_icon->showMessage(title, message, icon, NoticeMs);
}

} // namespace legilo
