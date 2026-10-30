#pragma once

#include <QSet>
#include <QString>
#include <QStringList>
#include <QStringView>

namespace legilo {

// English words, for confirming de-hyphenation across line breaks
// (design 7.2.4). Matching ignores case.
class WordList
{
public:
    // The list compiled into Legilo: SCOWL's British and American lists at
    // the standard size, as Debian packages them (wbritish, wamerican),
    // without possessives. Loaded on first use.
    static const WordList &bundled();

    explicit WordList(const QStringList &words);

    bool contains(QStringView word) const;
    qsizetype size() const;

private:
    QSet<QString> m_words;
};

} // namespace legilo
