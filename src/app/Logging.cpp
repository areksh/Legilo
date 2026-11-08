#include "app/Logging.h"

namespace legilo {

Q_LOGGING_CATEGORY(lcApp, "legilo.app")

void applyLoggingDefaults()
{
    QLoggingCategory::setFilterRules(QStringLiteral("legilo.*.debug=false"));
}

} // namespace legilo
