#include "doctest.h"
#include "core/mods/gamebanana_json.hpp"

using namespace thomaz::core;

static const char* SEARCH_JSON = R"({
  "_aMetadata": { "_nRecordCount": 158197, "_nPerpage": 15, "_bIsComplete": false },
  "_aRecords": [
    { "_idRow": 445087, "_sModelName": "Mod", "_sName": "Cool Skin",
      "_sProfileUrl": "https://gamebanana.com/mods/445087",
      "_bHasFiles": true, "_nLikeCount": 12, "_nViewCount": 3400 },
    { "_idRow": 999, "_sModelName": "Mod", "_sName": "Zero Likes Mod",
      "_sProfileUrl": "https://gamebanana.com/mods/999",
      "_bHasFiles": false }
  ]
})";

static const char* FILES_JSON = R"({
  "_aFiles": [
    { "_idRow": 1719374, "_sFile": "escambio.zip", "_nFilesize": 1048576,
      "_sMd5Checksum": "abc123", "_sDownloadUrl": "https://gamebanana.com/dl/1719374" },
    { "_idRow": 1719375, "_sFile": "extra.7z", "_nFilesize": 2048,
      "_sMd5Checksum": "def456", "_sDownloadUrl": "https://gamebanana.com/dl/1719375" }
  ]
})";

static const char* FILES_ERROR_JSON = R"({
  "_sErrorCode": "NO_SUCH_RECORD", "_sErrorMessage": "This Mod doesn't exist"
})";

TEST_CASE("parse_search_page reads metadata and records") {
    SearchPage p = parse_search_page(SEARCH_JSON);
    CHECK(p.total == 158197);
    CHECK(p.per_page == 15);
    CHECK(p.is_complete == false);
    REQUIRE(p.records.size() == 2);
    CHECK(p.records[0].id == 445087);
    CHECK(p.records[0].name == "Cool Skin");
    CHECK(p.records[0].has_files == true);
    CHECK(p.records[0].likes == 12);
    CHECK(p.records[0].views == 3400);
}

TEST_CASE("parse_search_page defaults optional counts to zero") {
    SearchPage p = parse_search_page(SEARCH_JSON);
    REQUIRE(p.records.size() == 2);
    CHECK(p.records[1].likes == 0);
    CHECK(p.records[1].views == 0);
    CHECK(p.records[1].has_files == false);
}

TEST_CASE("parse_search_page tolerates malformed json (empty page)") {
    SearchPage p = parse_search_page("not json");
    CHECK(p.records.empty());
    CHECK(p.total == 0);
}

TEST_CASE("parse_mod_files reads the file list") {
    ModFilesResult r = parse_mod_files(FILES_JSON);
    REQUIRE(r.ok);
    REQUIRE(r.files.size() == 2);
    CHECK(r.files[0].file_id == 1719374);
    CHECK(r.files[0].filename == "escambio.zip");
    CHECK(r.files[0].filesize == 1048576);
    CHECK(r.files[0].md5 == "abc123");
    CHECK(r.files[0].download_url == "https://gamebanana.com/dl/1719374");
}

TEST_CASE("parse_mod_files surfaces the HTTP-200 error body") {
    ModFilesResult r = parse_mod_files(FILES_ERROR_JSON);
    CHECK_FALSE(r.ok);
    CHECK(r.error == "This Mod doesn't exist");
    CHECK(r.files.empty());
}

TEST_CASE("parse_mod_files on malformed json fails cleanly") {
    ModFilesResult r = parse_mod_files("{");
    CHECK_FALSE(r.ok);
    CHECK(r.files.empty());
}
