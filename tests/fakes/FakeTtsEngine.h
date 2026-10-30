#pragma once

#include "core/tts/TtsEngine.h"

#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QStringList>

namespace legilo::testing {

// A TtsEngine that returns a constant tone of known length instantly: 10
// frames per character at 1000 Hz, so 100 characters make a second, divided
// by the speed. It records every call, and fails for text containing
// "FAIL". Safe to call from the synthesis worker's thread.
class FakeTtsEngine : public TtsEngine
{
public:
    static constexpr int Rate = 1000;
    static constexpr int FramesPerChar = 10;

    struct Call
    {
        QString text;
        double speed = 1.0;
    };

    static qsizetype framesFor(const QString &text, double speed = 1.0)
    {
        return qRound(static_cast<double>(text.size() * FramesPerChar) / speed);
    }

    int sampleRate() const override { return Rate; }

    std::optional<std::vector<float>> synthesise(const QString &text, double speed) override
    {
        {
            const QMutexLocker locker(&m_mutex);
            m_calls.append({text, speed});
        }
        if (text.contains(u"FAIL")) {
            return std::nullopt;
        }
        return std::vector<float>(framesFor(text, speed), 0.5F);
    }

    QList<Call> calls() const
    {
        const QMutexLocker locker(&m_mutex);
        return m_calls;
    }

    QStringList texts() const
    {
        QStringList texts;
        for (const Call &call : calls()) {
            texts.append(call.text);
        }
        return texts;
    }

private:
    mutable QMutex m_mutex;
    QList<Call> m_calls;
};

} // namespace legilo::testing
