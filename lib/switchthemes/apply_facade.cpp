#include "apply_facade.hpp"

#include <variant>
#include <stdexcept>

#include "MyTypes.h"
#include "NXTheme.hpp"
#include "Patcher.hpp"
#include "Common.hpp"
#include "SarcLib/Sarc.hpp"
#include "SarcLib/Yaz0.hpp"
#include "Layouts/Patches.hpp"

// All upstream-API knowledge lives in this file. The structure mirrors the apply
// portion of SwitchThemesNX/source/Pages/ThemeEntry/NxEntry.cpp::DoInstall, minus
// the firmware-extraction / filesystem / dialog glue we do not vendor.
namespace switchthemes {

namespace {

// Yaz0 + SARC unpack of a base .szs (matches NxEntry.cpp's SarcOpen helper).
SARC::SarcData SarcOpen(const std::vector<u8>& data)
{
    auto raw = Yaz0::Decompress(data);
    return SARC::Unpack(raw);
}

// SARC pack + Yaz0 compress (matches NxEntry.cpp's SarcPack helper).
std::vector<u8> SarcPack(SARC::SarcData& data)
{
    auto packed = SARC::Pack(data);
    return Yaz0::Compress(packed.data, 3, packed.align);
}

} // namespace

ApplyOutput apply_nxtheme(const std::vector<unsigned char>& base_szs,
                          const std::vector<unsigned char>& nxtheme)
{
    ApplyOutput out;
    try {
        if (base_szs.empty())
            throw std::runtime_error("base szs is empty");
        if (nxtheme.empty())
            throw std::runtime_error("nxtheme is empty");

        // 1. Parse the .nxtheme container (handles both ZIP and legacy Yaz0+SARC).
        NxTheme theme = NxTheme::TryLoad(nxtheme);
        if (!theme.IsValid())
            throw std::runtime_error(theme.error.value_or("invalid nxtheme"));

        // 2. Open the base layout and build a patcher over it.
        SARC::SarcData sarc = SarcOpen(base_szs);
        SwitchThemesCommon::SzsPatcher patcher(sarc);

        // The patcher must recognise this szs as a patchable target.
        if (!patcher.DetectedSarc().has_value())
            throw std::runtime_error("the base layout is not a recognised/patchable szs");

        bool patched = false;

        // 3a. Background (BNTX) patch. GetMainImage returns DDS bytes or an error string.
        if (theme.HasMainImage())
        {
            auto image = theme.GetMainImage();
            if (std::holds_alternative<std::string>(image))
                throw std::runtime_error("failed to decode background image: " +
                                         std::get<std::string>(image));

            if (!patcher.PatchMainBG(std::get<FileData>(image)))
                throw std::runtime_error("PatchMainBG (background) failed");

            patched = true;
        }

        // 3b. Layout (BFLYT/BFLAN) patch. The manifest Target names the part
        //     (home/lock/apps/...) which drives part-specific layout handling.
        if (theme.HasMainLayout())
        {
            const std::string partName = theme.manifest->Target;
            LayoutPatch patch = Patches::LoadLayout(theme.GetMainLayout());

            if (!patcher.PatchLayouts(patch, partName))
                throw std::runtime_error("PatchLayouts failed for target '" + partName + "'");

            patched = true;
        }

        if (!patched)
            throw std::runtime_error("nxtheme contained neither a background nor a layout to apply");

        // 4. Collect compatibility warnings (parts the engine dropped to avoid crashes).
        if (patcher.TotalNonCompatibleFixes > 0)
            out.warnings.push_back(
                "This theme contained a custom layout that was not fully compatible "
                "with the target firmware; " + std::to_string(patcher.TotalNonCompatibleFixes) +
                " incompatible part(s) were automatically removed and the look may change.");

        // 5. Serialize the patched SARC back into a .szs.
        SARC::SarcData& finalSarc = patcher.GetFinalSarc();
        out.szs = SarcPack(finalSarc);
        out.ok = true;
    } catch (const std::exception& e) {
        out.ok = false;
        out.error = e.what();
    } catch (...) {
        out.ok = false;
        out.error = "unknown theme engine error";
    }
    return out;
}

} // namespace switchthemes
