#include "app/SettingsDialog.h"

#include "app/Settings.h"
#include "app/ShortcutActions.h"
#include "platform/Autostart.h"
#include "platform/GlobalShortcuts.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

using namespace Qt::StringLiterals;

namespace legilo {

namespace {

QCheckBox *checkBox(const QString &name, const QString &text, bool checked, QWidget *parent)
{
    auto *box = new QCheckBox(text, parent);
    box->setObjectName(name);
    box->setChecked(checked);
    return box;
}

// A list of choices, each with its stored name as the item's data.
QComboBox *choices(const QString &name, const QList<std::pair<QString, QString>> &items,
                   const QString &current, QWidget *parent)
{
    auto *box = new QComboBox(parent);
    box->setObjectName(name);
    for (const auto &[text, data] : items) {
        box->addItem(text, data);
    }
    box->setCurrentIndex(std::max(box->findData(current), 0));
    return box;
}

// Calls set with the chosen item's data whenever the choice changes.
void onChoice(QComboBox *box, QObject *context, const std::function<void(const QString &)> &set)
{
    QObject::connect(box, &QComboBox::currentIndexChanged, context,
                     [box, set] { set(box->currentData().toString()); });
}

QString reflowModeName(ReflowMode mode)
{
    switch (mode) {
    case ReflowMode::Always:
        return u"always"_s;
    case ReflowMode::Never:
        return u"never"_s;
    case ReflowMode::Auto:
        break;
    }
    return u"auto"_s;
}

QString urlRuleName(UrlRule rule)
{
    switch (rule) {
    case UrlRule::Full:
        return u"full"_s;
    case UrlRule::Remove:
        return u"remove"_s;
    case UrlRule::Host:
        break;
    }
    return u"host"_s;
}

QString citationRuleName(CitationRule rule)
{
    switch (rule) {
    case CitationRule::Never:
        return u"never"_s;
    case CitationRule::Always:
        return u"always"_s;
    case CitationRule::PdfOnly:
        break;
    }
    return u"pdfOnly"_s;
}

// The options back from the names the choices store.
ReflowMode reflowModeNamed(const QString &name)
{
    if (name == "always"_L1) {
        return ReflowMode::Always;
    }
    if (name == "never"_L1) {
        return ReflowMode::Never;
    }
    return ReflowMode::Auto;
}

UrlRule urlRuleNamed(const QString &name)
{
    if (name == "full"_L1) {
        return UrlRule::Full;
    }
    if (name == "remove"_L1) {
        return UrlRule::Remove;
    }
    return UrlRule::Host;
}

CitationRule citationRuleNamed(const QString &name)
{
    if (name == "always"_L1) {
        return CitationRule::Always;
    }
    if (name == "never"_L1) {
        return CitationRule::Never;
    }
    return CitationRule::PdfOnly;
}

} // namespace

SettingsDialog::SettingsDialog(Settings *settings, GlobalShortcuts *shortcuts, Autostart *autostart,
                               const QList<Voice> &voices, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_shortcuts(shortcuts)
    , m_autostart(autostart)
{
    setWindowTitle(tr("Legilo Settings"));
    auto *tabs = new QTabWidget(this);
    tabs->addTab(generalPage(), tr("General"));
    tabs->addTab(voicePage(voices), tr("Voice"));
    tabs->addTab(shortcutsPage(), tr("Shortcuts"));
    tabs->addTab(cleanupPage(), tr("Clean-up"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);
}

QWidget *SettingsDialog::generalPage()
{
    auto *page = new QWidget(this);

    auto *autostart =
        checkBox(u"autostart"_s, tr("Start Legilo when I log in"), m_autostart->isEnabled(), page);
    connect(autostart, &QCheckBox::toggled, this, [this, autostart](bool on) {
        if (!m_autostart->setEnabled(on)) {
            const QSignalBlocker blocker(autostart);
            autostart->setChecked(!on);
            return;
        }
        m_settings->setAutostart(on);
    });

    auto *notifications = new QGroupBox(tr("Notifications"), page);
    auto *notifyStart = checkBox(u"notifyStart"_s, tr("When speech starts, show its first words"),
                                 m_settings->notifyStart(), notifications);
    connect(notifyStart, &QCheckBox::toggled, m_settings, &Settings::setNotifyStart);
    auto *notifySource =
        checkBox(u"notifySource"_s, tr("Say whether the selection or the clipboard was read"),
                 m_settings->notifySource(), notifications);
    connect(notifySource, &QCheckBox::toggled, m_settings, &Settings::setNotifySource);
    auto *notifyPause = checkBox(u"notifyPause"_s, tr("When speech pauses"),
                                 m_settings->notifyPause(), notifications);
    connect(notifyPause, &QCheckBox::toggled, m_settings, &Settings::setNotifyPause);
    auto *notificationsLayout = new QVBoxLayout(notifications);
    notificationsLayout->addWidget(notifyStart);
    notificationsLayout->addWidget(notifySource);
    notificationsLayout->addWidget(notifyPause);
    auto *notice = new QLabel(tr("Problems are always shown."), notifications);
    notice->setEnabled(false);
    notificationsLayout->addWidget(notice);

    const bool restart = m_settings->speakWhileSpeaking() == Settings::SpeakWhileSpeaking::Restart;
    auto *whileSpeaking =
        choices(u"speakWhileSpeaking"_s,
                {{tr("Stop speaking"), u"stop"_s}, {tr("Read the new text instead"), u"restart"_s}},
                restart ? u"restart"_s : u"stop"_s, page);
    onChoice(whileSpeaking, this, [this](const QString &name) {
        m_settings->setSpeakWhileSpeaking(name == "restart"_L1
                                              ? Settings::SpeakWhileSpeaking::Restart
                                              : Settings::SpeakWhileSpeaking::Stop);
    });

    auto *fallback = checkBox(u"selectionFallbackToClipboard"_s,
                              tr("When nothing is selected, read the clipboard"),
                              m_settings->selectionFallbackToClipboard(), page);
    connect(fallback, &QCheckBox::toggled, m_settings, &Settings::setSelectionFallbackToClipboard);

    auto *form = new QFormLayout;
    form->addRow(tr("Speak shortcut pressed during speech:"), whileSpeaking);

    auto *layout = new QVBoxLayout(page);
    layout->addWidget(autostart);
    layout->addWidget(notifications);
    layout->addLayout(form);
    layout->addWidget(fallback);
    layout->addStretch();
    return page;
}

QWidget *SettingsDialog::voicePage(const QList<Voice> &voices)
{
    auto *page = new QWidget(this);

    auto *voice = new QComboBox(page);
    voice->setObjectName(u"voice"_s);
    for (const Voice &each : voices) {
        voice->addItem(each.displayName, each.id);
    }
    // No choice saved means the first installed voice (Appendix A).
    voice->setCurrentIndex(std::max(voice->findData(m_settings->defaultVoice()), 0));
    voice->setEnabled(!voices.isEmpty());
    onChoice(voice, this, [this](const QString &id) { m_settings->setDefaultVoice(id); });

    auto *speed = new QDoubleSpinBox(page);
    speed->setObjectName(u"speed"_s);
    speed->setRange(Settings::MinSpeed, Settings::MaxSpeed);
    speed->setSingleStep(0.1);
    speed->setDecimals(1);
    speed->setSuffix(tr(" times"));
    speed->setValue(m_settings->speed());
    connect(speed, &QDoubleSpinBox::valueChanged, m_settings, &Settings::setSpeed);

    auto *form = new QFormLayout(page);
    form->addRow(tr("Voice:"), voice);
    form->addRow(tr("Speed:"), speed);
    if (voices.isEmpty()) {
        form->addRow(new QLabel(tr("No voices are installed. Install one with "
                                   "legilo --install-voice."),
                                page));
    }
    return page;
}

QWidget *SettingsDialog::shortcutsPage()
{
    auto *page = new QWidget(this);
    auto *form = new QFormLayout;
    auto *problem = new QLabel(page);
    problem->setObjectName(u"shortcutProblem"_s);
    problem->setWordWrap(true);

    for (const ShortcutAction &action : shortcutActions()) {
        auto *edit = new QKeySequenceEdit(m_shortcuts->shortcut(action.id), page);
        edit->setObjectName(u"shortcut-"_s + action.id);
        edit->setMaximumSequenceLength(1);
        edit->setClearButtonEnabled(true);
        auto *reset = new QToolButton(page);
        reset->setObjectName(u"reset-"_s + action.id);
        reset->setText(tr("Default"));
        reset->setToolTip(action.defaultSequence.isEmpty()
                              ? tr("No shortcut")
                              : action.defaultSequence.toString(QKeySequence::NativeText));

        const QString id = action.id;
        const auto apply = [this, id, edit, problem](const QKeySequence &sequence) {
            problem->setText(applyShortcut(id, sequence));
            const QSignalBlocker blocker(edit);
            edit->setKeySequence(m_shortcuts->shortcut(id));
        };
        connect(edit, &QKeySequenceEdit::editingFinished, this,
                [edit, apply] { apply(edit->keySequence()); });
        // The clear button changes the sequence without finishing editing.
        connect(edit, &QKeySequenceEdit::keySequenceChanged, this,
                [apply](const QKeySequence &sequence) {
                    if (sequence.isEmpty()) {
                        apply(sequence);
                    }
                });
        connect(reset, &QToolButton::clicked, this,
                [apply, sequence = action.defaultSequence] { apply(sequence); });

        auto *row = new QHBoxLayout;
        row->addWidget(edit, 1);
        row->addWidget(reset);
        form->addRow(action.label + u':', row);
    }

    // Changes made in System Settings show here too (7.6.3).
    connect(m_shortcuts, &GlobalShortcuts::shortcutChanged, this,
            [page](const QString &id, const QKeySequence &sequence) {
                if (auto *edit = page->findChild<QKeySequenceEdit *>(u"shortcut-"_s + id)) {
                    const QSignalBlocker blocker(edit);
                    edit->setKeySequence(sequence);
                }
            });

    auto *layout = new QVBoxLayout(page);
    layout->addLayout(form);
    layout->addWidget(problem);
    auto *note = new QLabel(tr("These shortcuts can also be changed in System Settings."), page);
    note->setEnabled(false);
    layout->addWidget(note);
    layout->addStretch();
    return page;
}

// Applies a shortcut and returns why it could not be, or an empty string.
QString SettingsDialog::applyShortcut(const QString &id, const QKeySequence &sequence)
{
    if (sequence == m_shortcuts->shortcut(id)) {
        return {};
    }
    if (const QString problem = shortcutProblem(sequence); !problem.isEmpty()) {
        return problem;
    }
    // Taking over another application's shortcut is not offered (7.6.3).
    if (!sequence.isEmpty() && !m_shortcuts->isAvailable(sequence)) {
        return tr("%1 is already in use by another application.")
            .arg(sequence.toString(QKeySequence::NativeText));
    }
    if (!m_shortcuts->setShortcut(id, sequence)) {
        return tr("%1 could not be set.").arg(sequence.toString(QKeySequence::NativeText));
    }
    return {};
}

QWidget *SettingsDialog::cleanupPage()
{
    auto *page = new QWidget(this);
    const CleanupSettings current = m_settings->cleanup();
    // Each control changes one field of the stored clean-up settings.
    const auto change = [this](const std::function<void(CleanupSettings &)> &edit) {
        CleanupSettings value = m_settings->cleanup();
        edit(value);
        m_settings->setCleanup(value);
    };

    auto *mode = choices(u"reflowMode"_s,
                         {{tr("When the text looks copied from a PDF"), u"auto"_s},
                          {tr("Always"), u"always"_s},
                          {tr("Never"), u"never"_s}},
                         reflowModeName(current.reflowMode), page);
    onChoice(mode, this, [change](const QString &name) {
        change([&name](CleanupSettings &value) { value.reflowMode = reflowModeNamed(name); });
    });

    auto *pdfRules = new QGroupBox(tr("In PDF text"), page);
    auto *pageNumbers =
        checkBox(u"pageNumbers"_s, tr("Leave out page numbers"), current.pageNumbers, pdfRules);
    connect(pageNumbers, &QCheckBox::toggled, this, [change](bool on) {
        change([on](CleanupSettings &value) { value.pageNumbers = on; });
    });
    auto *repeatedLines = checkBox(u"repeatedLines"_s, tr("Leave out running headers and footers"),
                                   current.repeatedLines, pdfRules);
    connect(repeatedLines, &QCheckBox::toggled, this, [change](bool on) {
        change([on](CleanupSettings &value) { value.repeatedLines = on; });
    });
    auto *dehyphenate = checkBox(u"dehyphenate"_s, tr("Join words broken across lines"),
                                 current.dehyphenate, pdfRules);
    connect(dehyphenate, &QCheckBox::toggled, this, [change](bool on) {
        change([on](CleanupSettings &value) { value.dehyphenate = on; });
    });
    auto *pdfLayout = new QVBoxLayout(pdfRules);
    pdfLayout->addWidget(pageNumbers);
    pdfLayout->addWidget(repeatedLines);
    pdfLayout->addWidget(dehyphenate);

    auto *urls = choices(u"urls"_s,
                         {{tr("Say the site only"), u"host"_s},
                          {tr("Say them in full"), u"full"_s},
                          {tr("Leave them out"), u"remove"_s}},
                         urlRuleName(current.urls), page);
    onChoice(urls, this, [change](const QString &name) {
        change([&name](CleanupSettings &value) { value.urls = urlRuleNamed(name); });
    });
    auto *emails = choices(
        u"emails"_s, {{tr("Say the domain only"), u"host"_s}, {tr("Say them in full"), u"full"_s}},
        current.emails == EmailRule::Full ? u"full"_s : u"host"_s, page);
    onChoice(emails, this, [change](const QString &name) {
        change([&name](CleanupSettings &value) {
            value.emails = name == "full"_L1 ? EmailRule::Full : EmailRule::Host;
        });
    });
    auto *citations = choices(u"citations"_s,
                              {{tr("Leave them out of PDF text"), u"pdfOnly"_s},
                               {tr("Leave them out of all text"), u"always"_s},
                               {tr("Read them"), u"never"_s}},
                              citationRuleName(current.citations), page);
    onChoice(citations, this, [change](const QString &name) {
        change([&name](CleanupSettings &value) { value.citations = citationRuleNamed(name); });
    });
    auto *authorYear = checkBox(u"authorYearCitations"_s,
                                tr("Leave out author-year citations, such as (Smith, 2020)"),
                                current.authorYearCitations, page);
    connect(authorYear, &QCheckBox::toggled, this, [change](bool on) {
        change([on](CleanupSettings &value) { value.authorYearCitations = on; });
    });

    auto *form = new QFormLayout;
    form->addRow(tr("Treat text as copied from a PDF:"), mode);
    form->addRow(tr("Web addresses:"), urls);
    form->addRow(tr("Email addresses:"), emails);
    form->addRow(tr("Numbered citations, such as [12]:"), citations);

    auto *layout = new QVBoxLayout(page);
    layout->addLayout(form);
    layout->addWidget(pdfRules);
    layout->addWidget(authorYear);
    layout->addStretch();
    return page;
}

} // namespace legilo
