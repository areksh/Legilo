#pragma once

#include <QString>

namespace legilo {

// Installs a voice model from one of sherpa-onnx's model archives (such as
// vits-piper-en_GB-cori-medium.tar.bz2) into the voices directory, where
// VoiceRegistry finds it. Serves both `legilo --install-voice` and, from
// stage 4, the model manager.
//
// The archive is unpacked into a ".install-*" directory, which the registry
// ignores, then given its legilo-voice.json manifest and renamed into place,
// so a voice appears whole or not at all. Only regular files and
// directories are accepted: an entry that is absolute, climbs out with
// "..", or is a link or a device fails the install. The model's own copy of
// espeak-ng's data is skipped, because Legilo uses the system's.
class ModelInstaller
{
public:
    struct Result
    {
        QString modelId; // the installed model's ID; empty on failure
        QString error;   // why the install failed; empty on success

        bool ok() const { return error.isEmpty(); }
    };

    // Installs into voicesDirectory, refusing archives that unpack to more
    // than maxBytes.
    explicit ModelInstaller(QString voicesDirectory, qint64 maxBytes = DefaultMaxBytes);

    // Installs the model in archivePath. The model's ID is the archive's
    // single top-level directory, or else its file name, without the
    // extension and with sherpa-onnx's "vits-" prefix removed. A model
    // with that ID already installed is left alone and reported as an
    // error.
    Result install(const QString &archivePath) const;

    // Well above the largest English model, Kokoro at about 350 MB.
    static constexpr qint64 DefaultMaxBytes = qint64(2) * 1024 * 1024 * 1024;

private:
    QString m_voicesDirectory;
    qint64 m_maxBytes;
};

} // namespace legilo
