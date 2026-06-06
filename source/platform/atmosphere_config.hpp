#pragma once

namespace thomaz {

// Result of ensuring Atmosphère is configured to load cheats enabled.
enum class CheatsDefaultResult {
    AlreadyEnabled, // the setting was already on — nothing changed
    Enabled,        // we just turned it on — a console reboot is required
    Failed,         // couldn't read/write system_settings.ini
};

// Ensure /atmosphere/config/system_settings.ini contains
//   [atmosphere]
//   dmnt_cheats_enabled_by_default = u8!0x1
// so regular '[cheat]' entries load ACTIVE instead of off-by-default. Without
// this, applying a cheat only makes it *available* — the user still has to flip
// it on in an overlay (EdiZon/Breeze). The edit is idempotent and preserves every
// other line/section in the file. Atmosphère reads system_settings.ini at boot,
// so a freshly-flipped setting only takes effect after a console reboot — that's
// what the Enabled result signals to the caller.
CheatsDefaultResult ensure_cheats_enabled_by_default();

} // namespace thomaz
