#include "core/text/WordList.h"

#include "core/Logging.h"

#include <QFile>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

// Compiled in from Debian's wbritish and wamerican (src/core/CMakeLists.txt).
const QStringList BundledFiles{u":/legilo/words/british-english"_s,
                               u":/legilo/words/american-english"_s};

} // namespace

const WordList &WordList::bundled()
{
    static const WordList list = [] {
        QStringList words;
        for (const QString &name : BundledFiles) {
            QFile file(name);
            if (!file.open(QIODevice::ReadOnly)) {
                qCWarning(lcText) << "Cannot read the word list" << name;
                continue;
            }
            for (const QByteArray &line : file.readAll().split('\n')) {
                // Possessives ("colour's") add nothing for de-hyphenation.
                if (!line.isEmpty() && !line.contains('\'')) {
                    words.append(QString::fromUtf8(line));
                }
            }
        }
        return WordList(words);
    }();
    return list;
}

WordList::WordList(const QStringList &words)
{
    m_words.reserve(words.size());
    for (const QString &word : words) {
        m_words.insert(word.toLower());
    }
}

bool WordList::contains(QStringView word) const
{
    return m_words.contains(word.toString().toLower());
}

qsizetype WordList::size() const
{
    return m_words.size();
}

} // namespace legilo
