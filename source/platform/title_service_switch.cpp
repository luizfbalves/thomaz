#include "platform/title_service_switch.hpp"

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

// Read the installed application version for one title (the base-app meta entry).
std::uint32_t readVersion(std::uint64_t application_id) {
    NsApplicationContentMetaStatus status[16];
    s32 count = 0;
    Result rc = nsListApplicationContentMetaStatus(application_id, 0, status,
                                                   (s32)(sizeof(status) / sizeof(status[0])), &count);
    if (R_FAILED(rc))
        return 0;
    // Prefer the base application meta entry; fall back to the first entry.
    for (s32 i = 0; i < count; i++) {
        if (status[i].meta_type == NcmContentMetaType_Application)
            return status[i].version;
    }
    return count > 0 ? status[0].version : 0;
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
