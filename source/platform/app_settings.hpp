#pragma once
#include <string>

namespace thomaz {

// Persisted app settings (currently just the UI locale). Stored as a tiny file
// on the SD (Switch) or working dir (desktop). The locale is applied at startup
// because Borealis loads translations once during init.

// Returns the saved locale ("auto", "en-US", "pt-BR"); "auto" if none saved.
std::string load_locale();

// Persist the chosen locale. Takes effect on the next launch.
void save_locale(const std::string& locale);

} // namespace thomaz
