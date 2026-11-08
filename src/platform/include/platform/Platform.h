#pragma once

#include <memory>

namespace legilo {

class GlobalShortcuts;
class TextSource;

// The platform's implementations of the interfaces, chosen at run time
// where a platform has more than one (design 6.1). Each needs the
// QGuiApplication to exist.
std::unique_ptr<TextSource> createTextSource();
std::unique_ptr<GlobalShortcuts> createGlobalShortcuts();

} // namespace legilo
