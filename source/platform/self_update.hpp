#pragma once
#include <string>

namespace thomaz {

// Remember how the app was launched (argv[0]); call once from main().
void set_self_path(const char* argv0);

// Path of the .nro to overwrite when updating: the launch path if it looks like
// an .nro, otherwise the canonical /switch/thomaz.nro.
std::string update_target_path();

} // namespace thomaz
