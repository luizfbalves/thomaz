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

// Returns the saved API base URL override, or the compiled default if none.
// Never has a trailing slash. Takes effect on next launch.
std::string load_api_base_url();

// Persist an API base URL override. Trims whitespace and any trailing slash.
// An empty value clears the override (falls back to the compiled default).
void save_api_base_url(const std::string& url);

} // namespace thomaz
