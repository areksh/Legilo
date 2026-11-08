#pragma once

#include "app/IpcProtocol.h"
#include "app/Settings.h"
#include "app/Speaker.h"
#include "app/TrayController.h"
#include "core/audio/MiniaudioOutput.h"
#include "core/playback/PlaybackController.h"

#include <QObject>
#include <QPointer>
#include <QThread>

#include <memory>

namespace legilo {

class GlobalShortcuts;
class TextSource;

// The running instance of Legilo: the tray, the global shortcuts and the
// speech behind them, wired together (design 6.1). There is one per user
// session (7.8); later launches reach it through handle().
class Instance : public QObject
{
    Q_OBJECT

public:
    explicit Instance(QObject *parent = nullptr);
    ~Instance() override;

    // Shows the tray icon, registers the shortcuts and starts loading the
    // voice in the background, so the tray is up at once (NFR-03).
    void start();

    // Carries out a command from this launch's command line or a later
    // launch's, and says whether it was accepted.
    IpcReply handle(const IpcRequest &request);

signals:
    void quitRequested();

private:
    void registerShortcuts();
    void onShortcut(const QString &id);
    void loadVoice();

    Settings m_settings;
    std::unique_ptr<TextSource> m_source;
    MiniaudioOutput m_output;
    PlaybackController m_controller;
    Speaker m_speaker;
    TrayController m_tray;
    std::unique_ptr<GlobalShortcuts> m_shortcuts;
    QPointer<QThread> m_loader;
};

} // namespace legilo
