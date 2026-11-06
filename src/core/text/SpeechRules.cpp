#include "core/text/SpeechRules.h"

#include <QRegularExpression>

#include <functional>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

constexpr auto Unicode = QRegularExpression::UseUnicodePropertiesOption;

using Replacement = std::function<QString(const QRegularExpressionMatch &)>;

// Replaces every match of pattern. An empty replacement also takes the
// whitespace before the match, or after it at the start, so "See [12]."
// becomes "See." rather than "See .".
QString replaceAll(const QString &text, const QRegularExpression &pattern,
                   const Replacement &replacement)
{
    QString result;
    qsizetype last = 0;
    for (const auto &match : pattern.globalMatch(text)) {
        QString spoken = replacement(match);
        qsizetype start = match.capturedStart();
        qsizetype end = match.capturedEnd();
        if (spoken.isEmpty()) {
            while (start > last && text[start - 1].isSpace()) {
                --start;
            }
            if (start == 0) {
                while (end < text.size() && text[end].isSpace()) {
                    ++end;
                }
            }
        }
        result += QStringView(text).sliced(last, start - last);
        result += spoken;
        last = end;
    }
    result += QStringView(text).sliced(last);
    return result;
}

// A URL ends before trailing punctuation, and before a closing bracket
// unless the URL opened it (fix 6: "/wiki/Foo_(bar)").
qsizetype urlLength(QStringView url)
{
    qsizetype length = url.size();
    while (length > 0) {
        const QChar c = url[length - 1];
        const QStringView kept = url.first(length);
        if (c == u')') {
            if (kept.count(u'(') >= kept.count(u')')) {
                break;
            }
        } else if (!u".,;:!?'\"]}"_s.contains(c)) {
            break;
        }
        --length;
    }
    return length;
}

QString hostOf(QStringView url)
{
    static const QRegularExpression scheme(u"^[a-z][a-z0-9+.-]*://"_s,
                                           QRegularExpression::CaseInsensitiveOption);
    QString host = url.toString().remove(scheme);
    host = host.section(u'@', -1); // user information
    for (const QChar end : {u'/', u'?', u'#', u':'}) {
        host = host.section(end, 0, 0);
    }
    if (host.startsWith(u"www."_s, Qt::CaseInsensitive)) {
        host.remove(0, 4);
    }
    return host;
}

QString speakUrls(const QString &text, UrlRule rule)
{
    if (rule == UrlRule::Full) {
        return text;
    }
    static const QRegularExpression url(u"(?<![\\w@.])(?:https?://|www\\.)[^\\s<>\"]+"_s,
                                        Unicode | QRegularExpression::CaseInsensitiveOption);
    QString result;
    qsizetype last = 0;
    for (const auto &match : url.globalMatch(text)) {
        const QStringView found = match.capturedView();
        const qsizetype length = urlLength(found);
        qsizetype start = match.capturedStart();
        QString spoken;
        if (rule == UrlRule::Host) {
            spoken = u"a link to "_s + hostOf(found.first(length));
        } else {
            while (start > last && text[start - 1].isSpace()) {
                --start;
            }
        }
        result += QStringView(text).sliced(last, start - last);
        result += spoken;
        last = match.capturedStart() + length;
    }
    result += QStringView(text).sliced(last);
    return result;
}

QString speakEmails(const QString &text, EmailRule rule)
{
    if (rule == EmailRule::Full) {
        return text;
    }
    static const QRegularExpression email(u"(?<![\\w.+-])[\\w.+-]+@((?:[\\w-]+\\.)+[\\w-]{2,})"_s,
                                          Unicode);
    return replaceAll(text, email, [](const QRegularExpressionMatch &match) {
        return u"an email address at "_s + match.captured(1);
    });
}

QString removeNumericCitations(const QString &text)
{
    static const QRegularExpression citation(
        u"\\[\\d+(?:\\s*[-\\x{2013}]\\s*\\d+)?(?:\\s*,\\s*\\d+(?:\\s*[-\\x{2013}]\\s*\\d+)?)*\\]"_s,
        Unicode);
    return replaceAll(text, citation, [](const QRegularExpressionMatch &) { return QString(); });
}

// "(Smith et al., 2020)", "(Smith and Jones 2019; Lee 2021a)".
QString removeAuthorYearCitations(const QString &text)
{
    const QString name = u"\\p{Lu}[\\p{L}'\\x{2019}-]+"_s;
    const QString names = name + u"(?:(?:,\\s*|\\s+and\\s+|\\s*&\\s*)"_s + name + u")*"_s;
    const QString entry = names + u"(?:\\s+et\\s+al\\.?)?,?\\s+\\d{4}[a-z]?"_s;
    static const QRegularExpression citation(u"\\("_s + entry + u"(?:;\\s*"_s + entry + u")*\\)"_s,
                                             Unicode);
    return replaceAll(text, citation, [](const QRegularExpressionMatch &) { return QString(); });
}

// espeak-ng reads U+00B2 and U+00B3 as "two" and "three", and a fraction
// after a digit without its "and"; the fractions alone it reads correctly.
QString speakSymbols(QString text)
{
    text.replace(QChar(0x00B2), u" squared"_s);
    text.replace(QChar(0x00B3), u" cubed"_s);
    static const QRegularExpression fraction(u"(\\d)([\\x{00BC}-\\x{00BE}])"_s, Unicode);
    return text.replace(fraction, u"\\1 and \\2"_s);
}

} // namespace

QStringList spokenSegments(const Document &document)
{
    QStringList spoken;
    for (const Segment &segment : document.segments) {
        const QStringView text =
            QStringView(document.cleaned).sliced(segment.start, segment.end - segment.start);
        spoken.append(speakable(text, document.settingsUsed, document.pdf));
    }
    return spoken;
}

QString spokenText(const Document &document)
{
    const QStringList spoken = spokenSegments(document);
    QString text;
    for (qsizetype i = 0; i < spoken.size(); ++i) {
        if (i > 0) {
            const bool newParagraph =
                document.segments.at(i).paragraph != document.segments.at(i - 1).paragraph;
            text += newParagraph ? u"\n\n"_s : u" "_s;
        }
        text += spoken.at(i);
    }
    return text;
}

QString speakable(QStringView text, const CleanupSettings &settings, bool pdf)
{
    QString spoken = speakUrls(text.toString(), settings.urls);
    spoken = speakEmails(spoken, settings.emails);
    if (settings.citations == CitationRule::Always ||
        (settings.citations == CitationRule::PdfOnly && pdf)) {
        spoken = removeNumericCitations(spoken);
    }
    if (settings.authorYearCitations) {
        spoken = removeAuthorYearCitations(spoken);
    }
    return speakSymbols(spoken);
}

} // namespace legilo
