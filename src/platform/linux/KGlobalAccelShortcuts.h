#pragma once

#include "platform/GlobalShortcuts.h"

#include <QHash>

class QAction;

namespace legilo {

// Global shortcuts kept by KDE's kglobalaccel (design 7.6.3), so they show
// in System Settings > Keyboard > Shortcuts under Legilo's name and can be
// changed there as well as in the app.
//
// Each action is a QAction whose object name is its ID, the key that
// kglobalaccel stores, under the component sh.arek.legilo, which matches
// the desktop file. Registering uses autoloading, so a shortcut the user
// chose before wins over the default.
class KGlobalAccelShortcuts : public GlobalShortcuts
{
    Q_OBJECT

public:
    explicit KGlobalAccelShortcuts(QObject *parent = nullptr);

    void registerAction(const QString &id, const QString &label,
                        const QKeySequence &defaultSequence) override;
    bool setShortcut(const QString &id, const QKeySequence &sequence) override;
    QKeySequence shortcut(const QString &id) const override;
    bool isAvailable(const QKeySequence &sequence) const override;

private:
    QHash<QString, QAction *> m_actions;
};

} // namespace legilo
