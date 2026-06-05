#pragma once
// Theme/firmware compatibility analysis.
//
// Custom-layout themes are authored for a specific firmware's menu layout. When
// applied to a much newer firmware (e.g. a fw-11.0 theme on a fw-22.1.0 console)
// the patched layout can crash qlaunch. Background-only themes are firmware-
// agnostic and always safe. This module classifies that risk BEFORE applying so
// the UI can warn and offer the safe "background only" fallback.
//
// Pure analysis (no __SWITCH__): uses the vendored theme engine, so it is
// host-testable. Only get_console_firmware() is platform-specific.
#include <string>
#include <vector>
#include "core/themes/themezer_types.hpp"

namespace thomaz {

struct FwVersion { unsigned major = 0, minor = 0, micro = 0; };

enum class CompatRisk { Safe, Caution, LikelyBroken };

struct PartCompat {
    std::string target;            // "ResidentMenu", "Entrance", ...
    bool        has_background = false;
    bool        has_layout     = false;
    int         target_firmware = 0; // LayoutPatch.TargetFirmware (1100 = 11.0); 0 if no layout
    CompatRisk  risk = CompatRisk::Safe;
    std::string detail;            // short engine/diagnostic note (may be empty)
};

struct ThemeCompat {
    CompatRisk              overall = CompatRisk::Safe;
    std::vector<PartCompat> parts;
    bool                    dry_run_done = false; // base layouts present => dry-run ran
};

// Classify ONE part from its raw .nxtheme bytes. base_szs empty => skip dry-run.
// Sets the engine firmware (hos::Version) to console_fw so the dry-run reflects
// the right firmware. Host-testable.
PartCompat analyze_part_compat(const std::vector<unsigned char>& nxtheme_bytes,
                               const std::vector<unsigned char>& base_szs,
                               FwVersion console_fw,
                               const std::string& target);

// Read the downloaded .nxtheme files for `detail` (and base layouts, if present,
// for the dry-run) and classify the whole theme/pack.
ThemeCompat analyze_theme_compat(const thomaz::core::ThemeDetail& detail,
                                 FwVersion console_fw, bool allow_dry_run);

// Console firmware (Switch: setsysGetFirmwareVersion; desktop: {0,0,0}).
FwVersion get_console_firmware();

// Set the engine's firmware (hos::Version) from the console so its firmware
// compatibility fixes actually run during apply. Idempotent.
void init_engine_firmware();

// "1100" -> "11.0"; FwVersion{22,1,0} -> "22.1.0".
std::string fw_int_to_string(int fw_enum);
std::string fw_to_string(FwVersion fw);

} // namespace thomaz
