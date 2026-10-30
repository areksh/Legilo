#include "app/SimpleSegmenter.h"

#include <QString>

namespace legilo {

namespace {

bool isSentenceEnd(QChar c)
{
    return c == u'.' || c == u'!' || c == u'?';
}

// Quotes and brackets that close a sentence stay with it.
bool isCloser(QChar c)
{
    return c == u'"' || c == u'\'' || c == u')' || c == u']' || c == QChar(0x2019) ||
           c == QChar(0x201D); // right single and double quotation marks
}

QStringList sentencesOf(const QString &paragraph)
{
    QStringList sentences;
    qsizetype start = 0;
    for (qsizetype i = 0; i < paragraph.size(); ++i) {
        if (!isSentenceEnd(paragraph[i])) {
            continue;
        }
        qsizetype end = i + 1;
        while (end < paragraph.size() && isCloser(paragraph[end])) {
            ++end;
        }
        if (end == paragraph.size() || paragraph[end].isSpace()) {
            sentences.append(paragraph.sliced(start, end - start).trimmed());
            start = end;
            i = end;
        }
    }
    if (const QString rest = paragraph.sliced(start).trimmed(); !rest.isEmpty()) {
        sentences.append(rest);
    }
    return sentences;
}

// A one-word sentence joins the one before it, or the next if it is first.
QStringList withoutLoneWords(const QStringList &sentences)
{
    QStringList segments;
    QString carried;
    for (const QString &sentence : sentences) {
        const bool loneWord = !sentence.contains(u' ');
        if (loneWord && !segments.isEmpty()) {
            segments.last() += u' ' + sentence;
        } else if (loneWord) {
            carried += carried.isEmpty() ? sentence : u' ' + sentence;
        } else {
            segments.append(carried.isEmpty() ? sentence : carried + u' ' + sentence);
            carried.clear();
        }
    }
    if (!carried.isEmpty()) {
        segments.append(carried);
    }
    return segments;
}

} // namespace

QStringList simpleSegments(QStringView text)
{
    QStringList segments;
    QStringList lines;
    const auto flush = [&] {
        const QString paragraph = lines.join(u' ').simplified();
        if (!paragraph.isEmpty()) {
            segments += withoutLoneWords(sentencesOf(paragraph));
        }
        lines.clear();
    };
    for (const QStringView line : text.split(u'\n')) {
        if (line.trimmed().isEmpty()) {
            flush();
        } else {
            lines.append(line.toString());
        }
    }
    flush();
    return segments;
}

} // namespace legilo
