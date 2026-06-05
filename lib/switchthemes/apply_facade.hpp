#pragma once
#include <string>
#include <vector>

namespace switchthemes {

struct ApplyOutput {
    bool ok = false;
    std::string error;                  // human message when !ok
    std::vector<unsigned char> szs;     // patched output when ok
    std::vector<std::string> warnings;  // incompatible parts the engine dropped
};

// Apply a .nxtheme differential onto a base firmware layout, returning the
// patched SZS bytes. base_szs/nxtheme are raw file contents.
//
// background_only: when true, apply ONLY the background image (PatchMainBG) and
// SKIP the custom layout (PatchLayouts). This is the safe fallback for themes
// whose layout is incompatible with the console firmware — a background swap is
// firmware-agnostic and cannot crash the menu, unlike a layout patch authored
// for an older firmware. If the theme has no background, the call fails.
ApplyOutput apply_nxtheme(const std::vector<unsigned char>& base_szs,
                          const std::vector<unsigned char>& nxtheme,
                          bool background_only = false);

} // namespace switchthemes
