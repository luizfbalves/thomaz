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
ApplyOutput apply_nxtheme(const std::vector<unsigned char>& base_szs,
                          const std::vector<unsigned char>& nxtheme);

} // namespace switchthemes
