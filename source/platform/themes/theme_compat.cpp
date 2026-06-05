#include "platform/themes/theme_compat.hpp"
#include "platform/themes/theme_paths.hpp"
#include "platform/themes/theme_download.hpp" // nxtheme_filename
#include "platform/themes/cfw_paths.hpp"      // base_szs_path

#include <fstream>
#include <sys/stat.h>

// Vendored theme engine (neutral on platform — compiles on host too).
#include "NXTheme.hpp"
#include "Common.hpp"               // SystemVersion, ConsoleFirmware, hos::Version
#include "Layouts/Patches.hpp"      // LayoutPatch, Patches::LoadLayout
#include "apply_facade.hpp"         // switchthemes::apply_nxtheme (dry-run)

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace thomaz {

namespace {

bool read_file(const std::string& path, std::vector<unsigned char>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

CompatRisk worse(CompatRisk a, CompatRisk b) {
    return (static_cast<int>(a) >= static_cast<int>(b)) ? a : b;
}

} // namespace

std::string fw_int_to_string(int fw) {
    if (fw <= 0) return "";
    int major = fw / 100;
    int minor = (fw / 10) % 10;
    int micro = fw % 10;
    std::string s = std::to_string(major) + "." + std::to_string(minor);
    if (micro) s += "." + std::to_string(micro);
    return s;
}

std::string fw_to_string(FwVersion fw) {
    return std::to_string(fw.major) + "." + std::to_string(fw.minor) + "." +
           std::to_string(fw.micro);
}

PartCompat analyze_part_compat(const std::vector<unsigned char>& nxtheme_bytes,
                               const std::vector<unsigned char>& base_szs,
                               FwVersion console_fw,
                               const std::string& target) {
    PartCompat pc;
    pc.target = target;

    // Make the engine think it is running on this console's firmware so its
    // firmware-compat fixes (and the dry-run below) behave correctly.
    hos::Version = SystemVersion{ console_fw.major, console_fw.minor, console_fw.micro };

    NxTheme theme = NxTheme::TryLoad(nxtheme_bytes);
    if (!theme.IsValid()) {
        pc.risk   = CompatRisk::LikelyBroken;
        pc.detail = theme.error.value_or("invalid nxtheme");
        return pc;
    }

    pc.has_background = theme.HasMainImage();
    pc.has_layout     = theme.HasMainLayout();

    // Background-only themes are firmware-agnostic: a texture swap can't crash
    // the menu. Always safe; no dry-run needed.
    if (!pc.has_layout) {
        pc.risk = CompatRisk::Safe;
        return pc;
    }

    // Custom layout: read the firmware it was authored for (informational only;
    // shown in the badge, not used to classify risk).
    try {
        LayoutPatch lp = Patches::LoadLayout(theme.GetMainLayout());
        pc.target_firmware = lp.TargetFirmware;
    } catch (...) {
        pc.target_firmware = 0;
    }

    // Risk is decided by the engine DRY-RUN, not by a firmware-version gap.
    // (The old "console fw > engine max -> Caution" heuristic was wrong: themes
    // authored for old firmware boot fine on 22.x once the qlaunch memory-budget
    // IPS patch is installed — see qlaunch_patches. The engine's own
    // apply_nxtheme is the real signal: a hard failure means it won't work;
    // warnings mean some parts were dropped.)
    if (!base_szs.empty()) {
        switchthemes::ApplyOutput ao =
            switchthemes::apply_nxtheme(base_szs, nxtheme_bytes, /*background_only*/ false);
        if (!ao.ok) {
            pc.risk   = CompatRisk::LikelyBroken;
            pc.detail = ao.error;
        } else if (!ao.warnings.empty()) {
            pc.risk = worse(pc.risk, CompatRisk::Caution);
            if (pc.detail.empty()) pc.detail = "engine_dropped_parts";
        }
    }

    return pc;
}

ThemeCompat analyze_theme_compat(const thomaz::core::ThemeDetail& detail,
                                 FwVersion console_fw, bool allow_dry_run) {
    ThemeCompat tc;
    std::string folder = theme_folder(detail.entry);

    int index = 0;
    for (const auto& part : detail.parts) {
        const int i = index++;
        if (part.target.empty()) continue;

        std::vector<unsigned char> nx_bytes;
        if (!read_file(folder + "/" + nxtheme_filename(part, i), nx_bytes)) {
            // Not downloaded yet — skip; the badge is only shown post-download.
            continue;
        }

        std::vector<unsigned char> base_bytes; // empty => no dry-run
        if (allow_dry_run) {
            std::string bp = base_szs_path(part.target);
            if (!bp.empty()) {
                struct stat st;
                if (::stat(bp.c_str(), &st) == 0) {
                    read_file(bp, base_bytes);
                    tc.dry_run_done = true;
                }
            }
        }

        PartCompat pc = analyze_part_compat(nx_bytes, base_bytes, console_fw, part.target);
        tc.overall = worse(tc.overall, pc.risk);
        tc.parts.push_back(std::move(pc));
    }

    return tc;
}

FwVersion get_console_firmware() {
    FwVersion v;
#ifdef __SWITCH__
    SetSysFirmwareVersion fw{};
    if (R_SUCCEEDED(setsysInitialize())) {
        if (R_SUCCEEDED(setsysGetFirmwareVersion(&fw))) {
            v.major = static_cast<unsigned>(fw.major);
            v.minor = static_cast<unsigned>(fw.minor);
            v.micro = static_cast<unsigned>(fw.micro);
        }
        setsysExit();
    }
#endif
    return v;
}

void init_engine_firmware() {
    FwVersion v = get_console_firmware();
    hos::Version = SystemVersion{ v.major, v.minor, v.micro };
}

} // namespace thomaz
