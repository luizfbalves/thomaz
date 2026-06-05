#include "szs_validate.hpp"

// SarcLib headers — included here, NOT in the neutral interface header (Pitfall 5).
// These libs are already linked via the Switch apply path and, after tests/Makefile
// wiring (02-02 Task 2), also in the host doctest build.
#include "SarcLib/Yaz0.hpp"
#include "SarcLib/Sarc.hpp"

namespace thomaz {

/// D-04 structural validation: a buffer is accepted only if it Yaz0-decompresses
/// (when Yaz0-compressed) and unpacks as a non-empty SARC archive.
///
/// This is a NEUTRAL translation unit — it includes no libnx headers,
/// so it compiles in both the Switch app (CMakeLists.txt glob) and the host doctest.
bool is_structurally_valid_szs(const std::vector<std::uint8_t>& buf) {
    if (buf.size() < 4) return false;
    try {
        // Copy into a local vector so we can replace it on Yaz0 decompress.
        std::vector<std::uint8_t> raw(buf.begin(), buf.end());

        // If the buffer has Yaz0 magic, decompress it first.
        // Yaz0::IsYaz0 checks the 4-byte "Yaz0" magic; Yaz0::Decompress throws on
        // corrupt/truncated Yaz0 data (caught below → returns false).
        if (Yaz0::IsYaz0(raw)) {
            raw = Yaz0::Decompress(raw);
        }

        // Attempt to unpack as a SARC archive.
        // SARC::Unpack throws on invalid SARC data (caught below → returns false).
        SARC::SarcData sd = SARC::Unpack(raw);

        // A valid layout szs must contain at least one file.
        return !sd.files.empty();
    } catch (...) {
        // Both Yaz0::Decompress and SARC::Unpack throw on corrupt input.
        // We catch everything and return false — the validator never throws to the caller.
        return false;
    }
}

} // namespace thomaz
