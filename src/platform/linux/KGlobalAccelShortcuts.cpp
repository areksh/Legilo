#include "linux/KGlobalAccelShortcuts.h"

#include "platform/Logging.h"

#include <KGlobalAccel>
#include <QAction>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

// The component the shortcuts are stored under: the application ID, so
// System Settings shows them with the desktop file's name and icon.
const auto ComponentName = u"sh.arek.legilo"_s;
const auto ComponentDisplayName = u"Legilo"_s;

} // namespace

KGlobalAccelShortcuts::KGlobalAccelShortcuts(QObject *parent)
    : GlobalShortcuts(parent)
{
    // Changes made in System Settings arrive here as well as those made in
    // the app, so both are reported the same way.
    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, this,
            [this](QAction *action, const QKeySequence &sequence) {
                if (m_actions.value(action->objectName()) == action) {
                    emit shortcutChanged(action->objectName(), sequence);
                }
            });
}

void KGlobalAccelShortcuts::registerAction(const QString &id, const QString &label,
                                           const QKeySequence &defaultSequence)
{
    auto *action = new QAction(label, this);
    action->setObjectName(id);
    action->setProperty("componentName", ComponentName);
    action->setProperty("componentDisplayName", ComponentDisplayName);
    connect(action, &QAction::triggered, this, [this, id] { emit activated(id); });
    m_actions.insert(id, action);

    QList<QKeySequence> defaults;
    if (!defaultSequence.isEmpty()) {
        defaults.append(defaultSequence);
    }
    KGlobalAccel::self()->setDefaultShortcut(action, defaults);
    // A default that another component already holds is left unassigned
    // (7.6.2): kglobalaccel gives the action no shortcut then.
    KGlobalAccel::self()->setShortcut(action, defaults);
    qCDebug(lcPlatform) << "Shortcut for" << id << "is" << shortcut(id).toString();
}

bool KGlobalAccelShortcuts::setShortcut(const QString &id, const QKeySequence &sequence)
{
    QAction *action = m_actions.value(id);
    if (action == nullptr) {
        return false;
    }
    if (!sequence.isEmpty() && !isAvailable(sequence) && shortcut(id) != sequence) {
        return false;
    }
    QList<QKeySequence> sequences;
    if (!sequence.isEmpty()) {
        sequences.append(sequence);
    }
    KGlobalAccel::self()->setShortcut(action, sequences, KGlobalAccel::NoAutoloading);
    return shortcut(id) == sequence;
}

QKeySequence KGlobalAccelShortcuts::shortcut(const QString &id) const
{
    QAction *action = m_actions.value(id);
    if (action == nullptr) {
        return {};
    }
    const QList<QKeySequence> sequences = KGlobalAccel::self()->shortcut(action);
    return sequences.isEmpty() ? QKeySequence() : sequences.first();
}

bool KGlobalAccelShortcuts::isAvailable(const QKeySequence &sequence) const
{
    return KGlobalAccel::isGlobalShortcutAvailable(sequence);
}

} // namespace legilo
