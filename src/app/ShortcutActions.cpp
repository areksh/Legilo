#include "app/ShortcutActions.h"

#include <QCoreApplication>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("ShortcutActions", text);
}

} // namespace

QList<ShortcutAction> shortcutActions()
{
    return {
        {u"speak-selection"_s, tr("Speak selection"), QKeySequence(u"Meta+Alt+S"_s)},
        {u"speak-clipboard"_s, tr("Speak clipboard"), QKeySequence(u"Meta+Alt+C"_s)},
        {u"speak-pdf-text"_s, tr("Speak selection as PDF text"), {}},
        {u"pause-resume"_s, tr("Pause or resume"), QKeySequence(u"Meta+Alt+P"_s)},
        {u"stop"_s, tr("Stop"), QKeySequence(u"Meta+Alt+X"_s)},
        {u"faster"_s, tr("Faster"), {}},
        {u"slower"_s, tr("Slower"), {}},
    };
}

QString shortcutProblem(const QKeySequence &sequence)
{
    if (sequence.isEmpty()) {
        return {};
    }
    if (sequence.count() > 1) {
        return tr("A global shortcut is a single key combination.");
    }
    const Qt::KeyboardModifiers modifiers = sequence[0].keyboardModifiers();
    if (!(modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        return tr("A global shortcut needs Ctrl, Alt or Meta; Shift alone is not enough.");
    }
    return {};
}

} // namespace legilo
