#pragma once
#include <string>
#include <vector>

namespace thomaz::core {

// Parsed contents of a sysmodule's toolbox.json.
struct ToolboxInfo {
    std::string name;             // friendly name; empty if not parseable
    bool requires_reboot = true;  // safe default: assume a reboot is needed
    bool valid = false;           // true only if json parsed into an object
};

// One folder found under /atmosphere/contents, with the facts the pure
// scanner needs. The platform layer fills this in from the real filesystem.
struct RawSysmoduleEntry {
    std::string program_id;        // folder name (16-hex), used verbatim
    bool has_exefs = false;        // exefs.nsp present -> this is a sysmodule
    bool has_boot2_flag = false;   // flags/boot2.flag present -> enabled at boot
    std::string toolbox_json;      // raw toolbox.json contents, empty if none
};

// A sysmodule presented to the user.
struct Sysmodule {
    std::string program_id;       // 16-hex folder name
    std::string name;             // toolbox name, or program_id when absent
    bool requires_reboot = true;  // from toolbox, default true
    bool enabled = false;         // boot2 flag present
    bool has_metadata = false;    // a valid toolbox.json was found
};

} // namespace thomaz::core
