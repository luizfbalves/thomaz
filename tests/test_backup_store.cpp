#include "doctest.h"
#include "core/backup_store.hpp"
#include <cstdio>
#include <sys/stat.h>
#include <fstream>

using namespace thomaz::core;

TEST_CASE("manifest round-trips through build + parse") {
    ManifestInfo in;
    in.game_name = "Zelda";
    in.title_id  = 0x0100000000010000ULL;
    in.timestamp = "2026-06-03_14-20-00";
    in.profiles  = {"e0e0...aa", "11112222"};

    std::string json = build_manifest(in);
    auto out = parse_manifest(json);

    REQUIRE(out.has_value());
    CHECK(out->game_name == "Zelda");
    CHECK(out->title_id  == 0x0100000000010000ULL);
    CHECK(out->timestamp == "2026-06-03_14-20-00");
    CHECK(out->profiles.size() == 2);
    CHECK(out->profiles[0] == "e0e0...aa");
    CHECK(out->profiles[1] == "11112222");
}

TEST_CASE("parse_manifest: title_id as a string is rejected; missing game_name is allowed") {
    // title_id stored as a string -> rejected (would otherwise silently become 0)
    CHECK_FALSE(parse_manifest(R"({"title_id":"0100","timestamp":"2026-01-01_00-00-00"})").has_value());
    // game_name optional: a manifest without it still parses, name defaults to ""
    auto ok = parse_manifest(R"({"title_id":4096,"timestamp":"2026-01-01_00-00-00"})");
    REQUIRE(ok.has_value());
    CHECK(ok->game_name == "");
    CHECK(ok->title_id == 4096);
}

TEST_CASE("parse_manifest returns nullopt on garbage") {
    CHECK_FALSE(parse_manifest("not json").has_value());
    CHECK_FALSE(parse_manifest("{}").has_value());
}

TEST_CASE("path builders compose root + lowercase title id + timestamp") {
    std::uint64_t tid = 0x0100000000010000ULL;
    CHECK(title_backups_dir("/sd/saves", tid) == "/sd/saves/0100000000010000");
    CHECK(backup_dir("/sd/saves", tid, "2026-06-03_14-20-00")
          == "/sd/saves/0100000000010000/2026-06-03_14-20-00");
}

TEST_CASE("format_timestamp_label renders dd/mm hh:mm") {
    CHECK(format_timestamp_label("2026-06-03_14-20-00") == "03/06 14:20");
    CHECK(format_timestamp_label("garbage") == "garbage");
}

static void write_file(const std::string& path, const std::string& body) {
    std::ofstream f(path, std::ios::binary);
    f << body;
}

TEST_CASE("list_backups reads manifests newest-first; last_backup_timestamp picks newest") {
    std::uint64_t tid = 0x0100000000020000ULL;
    std::string root = "test-saves-tmp";
    std::string tdir = root + "/0100000000020000";
    ::mkdir(root.c_str(), 0777);
    ::mkdir(tdir.c_str(), 0777);

    // Two backups, out of chronological order on disk.
    for (const char* ts : {"2026-06-01_10-00-00", "2026-06-03_14-20-00"}) {
        std::string b = tdir + "/" + ts;
        ::mkdir(b.c_str(), 0777);
        ManifestInfo m; m.title_id = tid; m.timestamp = ts; m.profiles = {"aa"};
        write_file(b + "/manifest.json", build_manifest(m));
    }
    // A junk subdir with no manifest — must be ignored.
    std::string junk = tdir + "/not-a-backup";
    ::mkdir(junk.c_str(), 0777);

    auto list = list_backups(root, tid);
    REQUIRE(list.size() == 2);
    CHECK(list[0].timestamp == "2026-06-03_14-20-00"); // newest first
    CHECK(list[1].timestamp == "2026-06-01_10-00-00");

    auto last = last_backup_timestamp(root, tid);
    REQUIRE(last.has_value());
    CHECK(*last == "2026-06-03_14-20-00");

    auto none = last_backup_timestamp(root, 0xDEADBEEFULL);
    CHECK_FALSE(none.has_value());
}

TEST_CASE("delete_backup removes one backup dir and its contents") {
    std::uint64_t tid = 0x0100000000030000ULL;
    std::string root = "test-saves-tmp";
    std::string tdir = root + "/0100000000030000";
    ::mkdir(root.c_str(), 0777);
    ::mkdir(tdir.c_str(), 0777);

    // Two backups so we can delete one and confirm the other survives.
    for (const char* ts : {"2026-06-01_10-00-00", "2026-06-03_14-20-00"}) {
        std::string b = tdir + "/" + ts;
        ::mkdir(b.c_str(), 0777);
        ManifestInfo m; m.title_id = tid; m.timestamp = ts; m.profiles = {"aa"};
        write_file(b + "/manifest.json", build_manifest(m));
        ::mkdir((b + "/aa").c_str(), 0777);
        write_file(b + "/aa/save.bin", "payload"); // nested file -> exercises recursion
    }

    auto before = list_backups(root, tid);
    REQUIRE(before.size() == 2);

    // Delete the newest; the directory (and nested files) must be gone.
    BackupEntry newest = before.front();
    CHECK(newest.timestamp == "2026-06-03_14-20-00");
    REQUIRE(delete_backup(newest));

    struct stat st;
    CHECK(::stat(newest.path.c_str(), &st) != 0); // dir no longer exists

    auto after = list_backups(root, tid);
    REQUIRE(after.size() == 1);
    CHECK(after[0].timestamp == "2026-06-01_10-00-00");

    // Empty path is a safe no-op.
    BackupEntry empty;
    CHECK_FALSE(delete_backup(empty));
}
