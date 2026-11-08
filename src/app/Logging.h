#pragma once

#include <QLoggingCategory>

namespace legilo {

Q_DECLARE_LOGGING_CATEGORY(lcApp)

// Turns Legilo's debug messages off, as design 7.16 has them by default:
// Qt shows every category's debug messages unless told otherwise. Rules
// in QT_LOGGING_RULES still apply over these, so "legilo.tts.debug=true"
// turns one back on. Call it once, at start-up.
void applyLoggingDefaults();

} // namespace legilo
