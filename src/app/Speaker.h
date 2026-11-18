#pragma once

#include "core/document/TextCapture.h"

#include <QObject>
#include <QStringList>

#include <memory>
#include <optional>

namespace legilo {

class PlaybackController;
class Settings;
class TextSource;
class TtsEngine;

// Carries out the speak actions of the shortcuts, the tray menu and the
// command line: captures the selection or the clipboard, cleans it up and
// hands it to the playback controller (design 6.4). It applies the
// settings that decide what a request does while speech is already
// playing (7.5), whether an empty selection falls back to the clipboard
// (FR-03), and which notifications are shown (7.7).
//
// The voice loads in the background at start-up (7.3.2). A request made
// before it is ready waits for setEngine() and then plays.
class Speaker : public QObject
{
    Q_OBJECT

public:
    // What a notification is about, which decides whether it is shown.
    enum class Notice
    {
        Started,
        Paused,
        Problem // always shown
    };
    Q_ENUM(Notice)

    // The source, controller and settings must outlive the speaker.
    Speaker(TextSource *source, PlaybackController *controller, Settings *settings,
            QObject *parent = nullptr);

    // The voice is ready. Plays a request that was waiting for it.
    void setEngine(std::shared_ptr<TtsEngine> engine);

    void speakSelection();
    // Speaks the selection with PDF reflow forced on, for PDF text that
    // detection misses.
    void speakSelectionAsPdfText();
    void speakClipboard();
    // Speaks the given text at once, replacing anything playing: text from
    // the command line or a file, where the speak-while-speaking setting,
    // which is about pressing a shortcut again, does not apply.
    void speakText(const TextCapture &capture);
    // Pauses speech that is playing and resumes speech that is paused.
    // With nothing playing, speaks the last text again from the sentence
    // where it stopped (design 7.5, togglePlay).
    void togglePause();
    // Changes the speed by a tenth, now and as the saved speed.
    void faster();
    void slower();
    void stop();

signals:
    // A notification to show, already filtered by the notification settings.
    void notify(legilo::Speaker::Notice notice, const QString &title, const QString &message);

private:
    // Reads the selection or the clipboard and speaks it, unless speech is
    // already under way and the settings say to stop it instead.
    void request(CaptureSource source, bool forcePdf = false);
    // Speaks a capture once the voice is ready. fellBack says the
    // selection was empty and this is the clipboard instead; forcePdf
    // turns PDF reflow on whatever detection says.
    void speak(const TextCapture &capture, bool fellBack, bool forcePdf);
    void changeSpeed(double step);
    void playWhenReady();
    bool isSpeaking() const;
    void notifyStarted(const TextCapture &capture, const QString &spoken, bool fellBack);

    TextSource *m_source;
    PlaybackController *m_controller;
    Settings *m_settings;
    std::shared_ptr<TtsEngine> m_engine;
    std::optional<QStringList> m_waiting; // spoken segments waiting for the voice
    QStringList m_last;                   // the segments spoken last
    int m_lastPosition = 0;               // where to replay them from
};

} // namespace legilo
