#include "platform/title_service_switch.hpp"

// Switch-only implementation. CMake globs every source file, so on desktop
// builds this translation unit must compile to nothing (no libnx there).
#ifdef __SWITCH__

#include <switch.h>
#include <cstring>

namespace thomaz {

bool NsTitleService::init() {
    return R_SUCCEEDED(nsInitialize());
}

void NsTitleService::exit() {
    nsExit();
}

namespace {

// Read the *effective* installed version for one title — the version of the
// program NSO that Atmosphère actually launches, which is what switch-cheats-db
// keys its build_ids on.
//
// A game with an update has TWO meta entries: an Application meta (the base,
// usually version 0) and a Patch meta (the update, version 0x10000, 0x20000, …).
// Atmosphère runs the *patched* program, whose main-NSO build_id maps to the
// PATCH version in versions.json — not the base. Reading the base version (as we
// used to) resolves the wrong build_id, so the cheat .txt is written under a
// filename that doesn't match the running binary and Atmosphère silently ignores
// it. Take the highest version across Application+Patch entries.
std::uint32_t readVersion(std::uint64_t application_id) {
    NsApplicationContentMetaStatus status[16];
    s32 count = 0;
    Result rc = nsListApplicationContentMetaStatus(application_id, 0, status,
                                                   (s32)(sizeof(status) / sizeof(status[0])), &count);
    if (R_FAILED(rc) || count <= 0)
        return 0;
    std::uint32_t best = 0;
    bool found = false;
    for (s32 i = 0; i < count; i++) {
        if (status[i].meta_type == NcmContentMetaType_Application ||
            status[i].meta_type == NcmContentMetaType_Patch) {
            if (!found || status[i].version > best) {
                best  = status[i].version;
                found = true;
            }
        }
    }
    return found ? best : status[0].version;
}

} // namespace

std::vector<InstalledTitle> NsTitleService::listInstalled() {
    std::vector<InstalledTitle> titles;

    // Page through installed application records.
    static constexpr s32 kPage = 32;
    NsApplicationRecord records[kPage];
    s32 offset = 0;

    // One control-data buffer reused per title (it is large: ~128KB + icon).
    NsApplicationControlData* control =
        (NsApplicationControlData*)malloc(sizeof(NsApplicationControlData));
    if (!control)
        return titles;

    while (true) {
        s32 recordCount = 0;
        Result rc = nsListApplicationRecord(records, kPage, offset, &recordCount);
        if (R_FAILED(rc) || recordCount <= 0)
            break;

        for (s32 i = 0; i < recordCount; i++) {
            InstalledTitle t;
            t.title_id = records[i].application_id;
            t.version  = readVersion(t.title_id);

            u64 controlSize = 0;
            Result crc = nsGetApplicationControlData(NsApplicationControlSource_Storage,
                                                     t.title_id, control,
                                                     sizeof(NsApplicationControlData), &controlSize);
            if (R_SUCCEEDED(crc) && controlSize >= sizeof(control->nacp)) {
                NacpLanguageEntry* entry = nullptr;
                if (R_SUCCEEDED(nacpGetLanguageEntry(&control->nacp, &entry)) && entry) {
                    t.name   = entry->name;
                    t.author = entry->author;
                }
                // Human-readable version string (e.g. "1.0.1"), separate from the
                // numeric meta `version` used for build_id resolution. NUL-bounded.
                char dv[sizeof(control->nacp.display_version) + 1] = {0};
                std::memcpy(dv, control->nacp.display_version, sizeof(control->nacp.display_version));
                t.display_version = dv;
                // Configured user-account save-data size (bytes); 0 if the game has none.
                t.save_data_size = control->nacp.user_account_save_data_size;
                // startup_user_account: 0 = no account required (typical for homebrew/forwarders).
                t.startup_user_account = control->nacp.startup_user_account;
                // The icon (JPEG) follows the nacp in the control data buffer.
                size_t iconSize = (size_t)controlSize - sizeof(control->nacp);
                if (iconSize > 0)
                    t.icon.assign(control->icon, control->icon + iconSize);
            }
            if (t.name.empty())
                t.name = "Unknown"; // still list the title even without control data

            titles.push_back(std::move(t));
        }

        offset += recordCount;
        if (recordCount < kPage)
            break;
    }

    free(control);
    return titles;
}

} // namespace thomaz

#endif // __SWITCH__
