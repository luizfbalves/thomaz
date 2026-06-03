#include "core/mods/gamebanana_json.hpp"

#include <nlohmann/json.hpp>

namespace thomaz::core {

using nlohmann::json;

SearchPage parse_search_page(const std::string& body) {
    SearchPage page;
    json doc = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded() || !doc.is_object())
        return page;

    if (doc.contains("_aMetadata") && doc["_aMetadata"].is_object()) {
        const json& m = doc["_aMetadata"];
        page.total       = m.value("_nRecordCount", (std::uint64_t)0);
        page.per_page    = m.value("_nPerpage", 0);
        page.is_complete = m.value("_bIsComplete", true);
    }

    if (doc.contains("_aRecords") && doc["_aRecords"].is_array()) {
        for (const json& r : doc["_aRecords"]) {
            if (!r.is_object())
                continue;
            ModRecord rec;
            rec.id          = r.value("_idRow", (std::uint64_t)0);
            rec.name        = r.value("_sName", std::string());
            rec.model       = r.value("_sModelName", std::string());
            rec.profile_url = r.value("_sProfileUrl", std::string());
            rec.has_files   = r.value("_bHasFiles", false);
            rec.likes       = r.value("_nLikeCount", (std::uint32_t)0);
            rec.views       = r.value("_nViewCount", (std::uint32_t)0);
            page.records.push_back(std::move(rec));
        }
    }
    return page;
}

ModFilesResult parse_mod_files(const std::string& body) {
    ModFilesResult result;
    json doc = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (doc.is_discarded() || !doc.is_object())
        return result;

    if (doc.contains("_sErrorCode")) {
        result.error = doc.value("_sErrorMessage", std::string("error"));
        return result;
    }

    if (doc.contains("_aFiles") && doc["_aFiles"].is_array()) {
        for (const json& f : doc["_aFiles"]) {
            if (!f.is_object())
                continue;
            ModFile mf;
            mf.file_id      = f.value("_idRow", (std::uint64_t)0);
            mf.filename     = f.value("_sFile", std::string());
            mf.filesize     = f.value("_nFilesize", (std::uint64_t)0);
            mf.md5          = f.value("_sMd5Checksum", std::string());
            mf.download_url = f.value("_sDownloadUrl", std::string());
            result.files.push_back(std::move(mf));
        }
    }
    result.ok = true;
    return result;
}

} // namespace thomaz::core
