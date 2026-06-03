# Save Backup & Restore (Phase 1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a Save Manager to thomaz that reads real Switch game saves, writes versioned backups to the SD card (all profiles, folder tree), and restores them back into a game's save.

**Architecture:** Mirrors the existing `ITitleService`/`IHttpClient` pattern — a pure `core/backup_store` (POSIX + JSON, no libnx, unit-tested on desktop), an `ISaveService` platform interface with a libnx Switch impl and a desktop fake impl, and two Borealis activities (list → detail). The SD folder structure is the source of truth for backup history; each backup carries a `manifest.json`.

**Tech Stack:** C++17, Borealis (UI), libnx (`fsdevMountSaveData`/`accountListAllUsers`/`fsdevCommitDevice`), nlohmann/json (`lib/json`), doctest (`tests/`), CMake (desktop + devkitPro Switch), Docker for the Switch build.

**Conventions for this codebase:**
- Tests are doctest. `tests/Makefile` globs `tests/*.cpp` + `../source/core/*.cpp`, so a new `source/core/backup_store.cpp` + `tests/test_backup_store.cpp` are picked up automatically. Build & run: `cd tests && make test`.
- CMake globs `source/*.cpp` recursively; new source files need no CMake edit. Switch-only translation units must compile to nothing on desktop via `#ifdef __SWITCH__` (see `title_service_switch.cpp`).
- Commit identity for this repo: `luizfbalves <luizzbanndera@gmail.com>` (NOT the ambient git email). Use `git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit ...`.
- Do NOT push or tag — local commits only until the user confirms stability.
- Desktop build/verify: `cmake -B build_desktop && make -C build_desktop -j$(nproc)`; smoke run `timeout 4 ./build_desktop/thomaz` (exit 124 = ran 4s, healthy).
- Switch build (Docker): `cp -rn lib/borealis/resources/* resources/` then `docker run --rm -v "$PWD":/work -w /work devkitpro/devkita64 bash -c 'cmake -B build_switch -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON; make -C build_switch thomaz.nro -j$(nproc)'`.

---

## File Structure

New files:
- `source/core/backup_store.hpp` / `.cpp` — shared data structs + pure path/manifest/history logic (POSIX dir scan + nlohmann JSON, no libnx). Unit-tested.
- `source/platform/save_service.hpp` — `ISaveService` interface (includes `core/backup_store.hpp` for shared structs).
- `source/platform/save_service_fake.hpp` / `.cpp` — desktop impl (writes dummy backups so the full UI flow runs without a console).
- `source/platform/save_service_switch.hpp` / `.cpp` — libnx impl (`#ifdef __SWITCH__`).
- `source/app/save_manager_activity.hpp` / `.cpp` — Screen 1 (game list + last-backup date).
- `source/app/save_detail_activity.hpp` / `.cpp` — Screen 2 (backup button + history with restore).
- `resources/xml/activity/save_manager.xml`, `resources/xml/activity/save_detail.xml`.
- `tests/test_backup_store.cpp` — unit tests for `backup_store`.

Modified files:
- `resources/i18n/{en-US,pt-BR,fr,ru,zh-Hans}/app.json` (or the existing i18n file) — `thomaz/saves/*` keys.
- `resources/xml/activity/home.xml` — make the Save Manager tile focusable + id'd.
- `source/app/home_activity.{hpp,cpp}` — accept `ISaveService*`, wire the tile to push `SaveManagerActivity`.
- `source/main.cpp` — construct the platform `ISaveService` and pass it to `HomeActivity`.

**Data ownership note:** shared structs (`SaveProfile`, `BackupEntry`, `ManifestInfo`) live in `core/backup_store.hpp` so that `platform/` depends on `core/` (never the reverse). This is a small, intentional deviation from the spec, which sketched them inside `save_service.hpp`.

---

## Task 1: Core data structs + manifest build/parse

**Files:**
- Create: `source/core/backup_store.hpp`
- Create: `source/core/backup_store.cpp`
- Test: `tests/test_backup_store.cpp`

- [ ] **Step 1: Write the header with structs and the manifest function declarations**

Create `source/core/backup_store.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace thomaz::core {

// A local console user profile that has save data.
struct SaveProfile {
    std::uint64_t uid = 0;   // AccountUid low/high folded to a 16-hex string elsewhere
    std::string uid_hex;     // 32-hex string form, used as the on-SD folder name
    std::string name;        // friendly nickname (display only)
};

// Contents of a backup's manifest.json (and the data needed to write one).
struct ManifestInfo {
    std::string game_name;
    std::uint64_t title_id = 0;
    std::string timestamp;             // "YYYY-MM-DD_HH-MM-SS"
    std::vector<std::string> profiles; // profile uid_hex strings included in the backup
};

// One backup on disk for a title.
struct BackupEntry {
    std::string path;                  // absolute dir of this backup
    std::string timestamp;             // "YYYY-MM-DD_HH-MM-SS"
    std::vector<std::string> profiles; // uid_hex strings present (from manifest)
};

// Serialize a manifest to a JSON string.
std::string build_manifest(const ManifestInfo& info);

// Parse a manifest.json body; nullopt if malformed.
std::optional<ManifestInfo> parse_manifest(const std::string& json);

} // namespace thomaz::core
```

- [ ] **Step 2: Write the failing test for manifest round-trip**

Create `tests/test_backup_store.cpp`:

```cpp
#include "doctest.h"
#include "core/backup_store.hpp"

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
}

TEST_CASE("parse_manifest returns nullopt on garbage") {
    CHECK_FALSE(parse_manifest("not json").has_value());
    CHECK_FALSE(parse_manifest("{}").has_value());
}
```

- [ ] **Step 3: Run the test to verify it fails**

Run: `cd tests && make test`
Expected: FAIL — link/compile error (`build_manifest`/`parse_manifest` undefined).

- [ ] **Step 4: Implement manifest build/parse**

Create `source/core/backup_store.cpp`:

```cpp
#include "core/backup_store.hpp"

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace thomaz::core {

std::string build_manifest(const ManifestInfo& info) {
    json j;
    j["game_name"] = info.game_name;
    j["title_id"]  = info.title_id;
    j["timestamp"] = info.timestamp;
    j["profiles"]  = info.profiles;
    return j.dump(2);
}

std::optional<ManifestInfo> parse_manifest(const std::string& body) {
    json j = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object())
        return std::nullopt;
    if (!j.contains("title_id") || !j.contains("timestamp"))
        return std::nullopt;

    ManifestInfo info;
    info.game_name = j.value("game_name", std::string{});
    info.title_id  = j.value("title_id", std::uint64_t{0});
    info.timestamp = j.value("timestamp", std::string{});
    if (j.contains("profiles") && j["profiles"].is_array())
        for (const auto& p : j["profiles"])
            if (p.is_string())
                info.profiles.push_back(p.get<std::string>());
    return info;
}

} // namespace thomaz::core
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `cd tests && make test`
Expected: PASS (all existing tests + the two new ones).

- [ ] **Step 6: Commit**

```bash
git add source/core/backup_store.hpp source/core/backup_store.cpp tests/test_backup_store.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(saves): core manifest build/parse for backups"
```

---

## Task 2: Core path builders + timestamp label

**Files:**
- Modify: `source/core/backup_store.hpp`
- Modify: `source/core/backup_store.cpp`
- Test: `tests/test_backup_store.cpp`

- [ ] **Step 1: Add declarations to the header**

Add to `source/core/backup_store.hpp` inside the namespace (after `parse_manifest`):

```cpp
// Root directory holding all backups. Platform-specific:
//   Switch  -> "/switch/thomaz/saves"
//   desktop -> "thomaz-saves" (relative to the working dir)
std::string saves_root();

// <root>/<titleIdLowerHex>  — all backups of one title live here.
std::string title_backups_dir(const std::string& root, std::uint64_t title_id);

// <root>/<titleIdLowerHex>/<timestamp>  — one specific backup.
std::string backup_dir(const std::string& root, std::uint64_t title_id,
                       const std::string& timestamp);

// "2026-06-03_14-20-00" -> "03/06 14:20" for display. Returns the input
// unchanged if it does not match the expected shape.
std::string format_timestamp_label(const std::string& timestamp);
```

- [ ] **Step 2: Write the failing tests**

Add to `tests/test_backup_store.cpp`:

```cpp
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
```

- [ ] **Step 3: Run to verify failure**

Run: `cd tests && make test`
Expected: FAIL (undefined `title_backups_dir` etc.).

- [ ] **Step 4: Implement**

Add to the top of `source/core/backup_store.cpp` (after the existing includes):

```cpp
#include "core/db_paths.hpp" // title_id_hex
```

Add these functions inside the namespace:

```cpp
std::string saves_root() {
#ifdef __SWITCH__
    return "/switch/thomaz/saves";
#else
    return "thomaz-saves";
#endif
}

std::string title_backups_dir(const std::string& root, std::uint64_t title_id) {
    return root + "/" + title_id_hex(title_id, /*upper=*/false);
}

std::string backup_dir(const std::string& root, std::uint64_t title_id,
                       const std::string& timestamp) {
    return title_backups_dir(root, title_id) + "/" + timestamp;
}

std::string format_timestamp_label(const std::string& ts) {
    // Expect "YYYY-MM-DD_HH-MM-SS" (19 chars).
    if (ts.size() != 19 || ts[4] != '-' || ts[7] != '-' || ts[10] != '_' ||
        ts[13] != '-' || ts[16] != '-')
        return ts;
    // "DD/MM HH:MM"
    return ts.substr(8, 2) + "/" + ts.substr(5, 2) + " " +
           ts.substr(11, 2) + ":" + ts.substr(14, 2);
}
```

- [ ] **Step 5: Run to verify pass**

Run: `cd tests && make test`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add source/core/backup_store.hpp source/core/backup_store.cpp tests/test_backup_store.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(saves): core path builders + timestamp label"
```

---

## Task 3: Core backup-history scanning

**Files:**
- Modify: `source/core/backup_store.hpp`
- Modify: `source/core/backup_store.cpp`
- Test: `tests/test_backup_store.cpp`

- [ ] **Step 1: Add declarations**

Add to `source/core/backup_store.hpp`:

```cpp
// List a title's backups, newest first. Reads each subdir's manifest.json.
// Subdirs without a valid manifest are skipped. Empty if none / dir missing.
std::vector<BackupEntry> list_backups(const std::string& root, std::uint64_t title_id);

// The newest backup's "YYYY-MM-DD_HH-MM-SS", or nullopt if there are none.
std::optional<std::string> last_backup_timestamp(const std::string& root,
                                                 std::uint64_t title_id);
```

- [ ] **Step 2: Write the failing test (builds a temp dir tree, then scans it)**

Add to `tests/test_backup_store.cpp`:

```cpp
#include <cstdio>
#include <sys/stat.h>
#include <fstream>

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
```

- [ ] **Step 3: Run to verify failure**

Run: `cd tests && make test`
Expected: FAIL (undefined `list_backups`).

- [ ] **Step 4: Implement the scan**

Add includes near the top of `source/core/backup_store.cpp`:

```cpp
#include "platform/cheat_store.hpp" // read_text_file
#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
```

Add functions inside the namespace:

```cpp
std::vector<BackupEntry> list_backups(const std::string& root, std::uint64_t title_id) {
    std::vector<BackupEntry> out;
    std::string dir = title_backups_dir(root, title_id);

    DIR* d = ::opendir(dir.c_str());
    if (!d)
        return out;

    struct dirent* e;
    while ((e = ::readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        std::string sub = dir + "/" + name;
        struct stat st;
        if (::stat(sub.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
            continue;
        auto body = thomaz::read_text_file(sub + "/manifest.json");
        if (!body)
            continue;
        auto info = parse_manifest(*body);
        if (!info)
            continue;
        BackupEntry entry;
        entry.path      = sub;
        entry.timestamp = info->timestamp;
        entry.profiles  = info->profiles;
        out.push_back(std::move(entry));
    }
    ::closedir(d);

    // Newest first. Timestamps are zero-padded "YYYY-MM-DD_HH-MM-SS",
    // so lexical descending == chronological descending.
    std::sort(out.begin(), out.end(),
              [](const BackupEntry& a, const BackupEntry& b) {
                  return a.timestamp > b.timestamp;
              });
    return out;
}

std::optional<std::string> last_backup_timestamp(const std::string& root,
                                                 std::uint64_t title_id) {
    auto list = list_backups(root, title_id);
    if (list.empty())
        return std::nullopt;
    return list.front().timestamp;
}
```

- [ ] **Step 5: Run to verify pass; then clean the temp dir**

Run: `cd tests && make test && rm -rf test-saves-tmp`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add source/core/backup_store.hpp source/core/backup_store.cpp tests/test_backup_store.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(saves): core backup-history scan + last-backup lookup"
```

---

## Task 4: ISaveService interface + desktop fake impl

**Files:**
- Create: `source/platform/save_service.hpp`
- Create: `source/platform/save_service_fake.hpp`
- Create: `source/platform/save_service_fake.cpp`

No unit test here (it touches the working-dir filesystem and time); it is verified end-to-end on desktop in Task 11. Build verification only.

- [ ] **Step 1: Write the interface**

Create `source/platform/save_service.hpp`:

```cpp
#pragma once

#include <string>
#include <vector>

#include "core/backup_store.hpp" // SaveProfile, BackupEntry
#include "platform/title.hpp"    // InstalledTitle

namespace thomaz {

// Reads/writes real game save data. Switch impl uses libnx; the fake impl
// lets the full UI flow run on desktop without a console.
class ISaveService {
  public:
    virtual ~ISaveService() = default;

    // Profiles that currently have save data for this title.
    virtual std::vector<core::SaveProfile> profilesWithSave(std::uint64_t title_id) = 0;

    // Back up every profile's save for this title to the SD. On success returns
    // true; on failure returns false and sets *outError to a human message.
    virtual bool backup(const InstalledTitle& title, std::string* outError) = 0;

    // Restore a backup folder back into the title's save (destructive). On
    // success returns true; on failure returns false and sets *outError.
    virtual bool restore(const core::BackupEntry& entry, std::uint64_t title_id,
                         std::string* outError) = 0;
};

} // namespace thomaz
```

- [ ] **Step 2: Write the fake header**

Create `source/platform/save_service_fake.hpp`:

```cpp
#pragma once

#ifndef __SWITCH__

#include "platform/save_service.hpp"

namespace thomaz {

// Desktop stand-in: writes dummy backup folders under saves_root() so the UI
// flow (list, last-backup date, history, restore) is exercisable without a console.
class FakeSaveService : public ISaveService {
  public:
    std::vector<core::SaveProfile> profilesWithSave(std::uint64_t title_id) override;
    bool backup(const InstalledTitle& title, std::string* outError) override;
    bool restore(const core::BackupEntry& entry, std::uint64_t title_id,
                 std::string* outError) override;
};

} // namespace thomaz

#endif // !__SWITCH__
```

- [ ] **Step 3: Write the fake impl**

Create `source/platform/save_service_fake.cpp`:

```cpp
#include "platform/save_service_fake.hpp"

#ifndef __SWITCH__

#include <ctime>

#include "core/backup_store.hpp"
#include "platform/cheat_store.hpp" // write_text_file

namespace thomaz {

namespace {
// "YYYY-MM-DD_HH-MM-SS" from local time.
std::string now_timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &tm);
    return buf;
}
} // namespace

std::vector<core::SaveProfile> FakeSaveService::profilesWithSave(std::uint64_t) {
    return {{1, "11111111111111111111111111111111", "Player One"}};
}

bool FakeSaveService::backup(const InstalledTitle& title, std::string* outError) {
    std::string root = core::saves_root();
    std::string ts   = now_timestamp();
    std::string dir  = core::backup_dir(root, title.title_id, ts);

    // One dummy profile folder + a dummy save file.
    std::string profileHex = "11111111111111111111111111111111";
    if (!write_text_file(dir + "/" + profileHex + "/save.dat", "fake save bytes")) {
        if (outError) *outError = "could not write to SD";
        return false;
    }

    core::ManifestInfo m;
    m.game_name = title.name;
    m.title_id  = title.title_id;
    m.timestamp = ts;
    m.profiles  = {profileHex};
    if (!write_text_file(dir + "/manifest.json", core::build_manifest(m))) {
        if (outError) *outError = "could not write manifest";
        return false;
    }
    return true;
}

bool FakeSaveService::restore(const core::BackupEntry& entry, std::uint64_t,
                              std::string* outError) {
    // Nothing real to write back on desktop; succeed if the backup exists.
    if (!read_text_file(entry.path + "/manifest.json")) {
        if (outError) *outError = "backup missing manifest";
        return false;
    }
    return true;
}

} // namespace thomaz

#endif // !__SWITCH__
```

- [ ] **Step 4: Build the desktop target to verify it compiles**

Run: `cmake -B build_desktop >/dev/null && make -C build_desktop -j$(nproc) 2>&1 | tail -5`
Expected: build succeeds (the new files compile; nothing links them yet — that's fine).

- [ ] **Step 5: Commit**

```bash
git add source/platform/save_service.hpp source/platform/save_service_fake.hpp source/platform/save_service_fake.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(saves): ISaveService interface + desktop fake impl"
```

---

## Task 5: Switch impl — backup (libnx)

**Files:**
- Create: `source/platform/save_service_switch.hpp`
- Create: `source/platform/save_service_switch.cpp`

This layer is not unit-testable (requires the console + libnx). Its "test" is a clean Docker compile plus the on-console smoke test in Task 12. Write the code, then compile it in Docker; if libnx signatures differ in this devkitPro version, check `$DEVKITPRO/libnx/include/switch/services/acc.h` and `.../fs_dev.h` and adjust.

- [ ] **Step 1: Write the Switch header**

Create `source/platform/save_service_switch.hpp`:

```cpp
#pragma once

#ifdef __SWITCH__

#include "platform/save_service.hpp"

namespace thomaz {

// libnx-backed save backup/restore.
class NsSaveService : public ISaveService {
  public:
    std::vector<core::SaveProfile> profilesWithSave(std::uint64_t title_id) override;
    bool backup(const InstalledTitle& title, std::string* outError) override;
    bool restore(const core::BackupEntry& entry, std::uint64_t title_id,
                 std::string* outError) override;
};

} // namespace thomaz

#endif // __SWITCH__
```

- [ ] **Step 2: Write the backup impl (plus shared libnx helpers)**

Create `source/platform/save_service_switch.cpp`:

```cpp
#include "platform/save_service_switch.hpp"

#ifdef __SWITCH__

#include <switch.h>

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <ctime>
#include <vector>

#include "core/backup_store.hpp"
#include "platform/cheat_store.hpp" // write_text_file

namespace thomaz {

namespace {

constexpr const char* kMount = "thomaz_save"; // mount name -> "thomaz_save:/"

std::string now_timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[20];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &tm);
    return buf;
}

// 32-hex string for an AccountUid (high then low), used as the SD folder name.
std::string uid_hex(AccountUid uid) {
    char buf[33];
    std::snprintf(buf, sizeof(buf), "%016lx%016lx",
                  (unsigned long)uid.uid[0], (unsigned long)uid.uid[1]);
    return buf;
}

// mkdir -p
void make_dirs(const std::string& path) {
    std::string acc;
    for (size_t i = 0; i < path.size(); ++i) {
        acc += path[i];
        if (path[i] == '/' && acc.size() > 1)
            ::mkdir(acc.c_str(), 0777);
    }
    ::mkdir(path.c_str(), 0777);
}

// Recursively copy everything under src dir into dst dir (both already exist).
bool copy_tree(const std::string& src, const std::string& dst) {
    DIR* d = ::opendir(src.c_str());
    if (!d)
        return false;
    bool ok = true;
    struct dirent* e;
    while (ok && (e = ::readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..")
            continue;
        std::string s = src + "/" + name;
        std::string t = dst + "/" + name;
        struct stat st;
        if (::stat(s.c_str(), &st) != 0) { ok = false; break; }
        if (S_ISDIR(st.st_mode)) {
            ::mkdir(t.c_str(), 0777);
            ok = copy_tree(s, t);
        } else {
            FILE* in = std::fopen(s.c_str(), "rb");
            FILE* out = std::fopen(t.c_str(), "wb");
            if (!in || !out) { ok = false; }
            char buf[8192];
            size_t n;
            while (ok && (n = std::fread(buf, 1, sizeof(buf), in)) > 0)
                if (std::fwrite(buf, 1, n, out) != n) ok = false;
            if (in) std::fclose(in);
            if (out) std::fclose(out);
        }
    }
    ::closedir(d);
    return ok;
}

// Recursively delete the contents of a directory (leaves the dir itself).
void clear_tree(const std::string& dir) {
    DIR* d = ::opendir(dir.c_str());
    if (!d) return;
    struct dirent* e;
    while ((e = ::readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        std::string p = dir + "/" + name;
        struct stat st;
        if (::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            clear_tree(p);
            ::rmdir(p.c_str());
        } else {
            ::remove(p.c_str());
        }
    }
    ::closedir(d);
}

std::vector<core::SaveProfile> all_profiles() {
    std::vector<core::SaveProfile> out;
    AccountUid uids[ACC_USER_LIST_SIZE];
    s32 count = 0;
    if (R_FAILED(accountListAllUsers(uids, ACC_USER_LIST_SIZE, &count)))
        return out;
    for (s32 i = 0; i < count; ++i) {
        core::SaveProfile p;
        p.uid_hex = uid_hex(uids[i]);
        AccountProfile profile;
        if (R_SUCCEEDED(accountGetProfile(&profile, uids[i]))) {
            AccountProfileBase base;
            if (R_SUCCEEDED(accountProfileGet(&profile, nullptr, &base)))
                p.name = base.nickname;
            accountProfileClose(&profile);
        }
        out.push_back(std::move(p));
    }
    return out;
}

} // namespace

std::vector<core::SaveProfile> NsSaveService::profilesWithSave(std::uint64_t title_id) {
    std::vector<core::SaveProfile> out;
    accountInitialize(AccountServiceType_System);
    for (auto& p : all_profiles()) {
        AccountUid uid;
        std::sscanf(p.uid_hex.c_str(), "%016lx%016lx",
                    (unsigned long*)&uid.uid[0], (unsigned long*)&uid.uid[1]);
        if (R_SUCCEEDED(fsdevMountSaveData(kMount, title_id, uid))) {
            out.push_back(p);
            fsdevUnmountDevice(kMount);
        }
    }
    accountExit();
    return out;
}

bool NsSaveService::backup(const InstalledTitle& title, std::string* outError) {
    accountInitialize(AccountServiceType_System);

    std::string ts  = now_timestamp();
    std::string dir = core::backup_dir(core::saves_root(), title.title_id, ts);

    std::vector<std::string> savedProfiles;
    bool anyFailure = false;

    for (auto& p : all_profiles()) {
        AccountUid uid;
        std::sscanf(p.uid_hex.c_str(), "%016lx%016lx",
                    (unsigned long*)&uid.uid[0], (unsigned long*)&uid.uid[1]);
        if (R_FAILED(fsdevMountSaveData(kMount, title.title_id, uid)))
            continue; // this profile has no save for this title

        std::string dst = dir + "/" + p.uid_hex;
        make_dirs(dst);
        std::string mountRoot = std::string(kMount) + ":/";
        if (copy_tree(mountRoot, dst))
            savedProfiles.push_back(p.uid_hex);
        else
            anyFailure = true;
        fsdevUnmountDevice(kMount);
    }
    accountExit();

    if (savedProfiles.empty()) {
        clear_tree(dir); ::rmdir(dir.c_str()); // remove partial/empty
        if (outError) *outError = anyFailure ? "copy failed" : "no save data";
        return false;
    }

    core::ManifestInfo m;
    m.game_name = title.name;
    m.title_id  = title.title_id;
    m.timestamp = ts;
    m.profiles  = savedProfiles;
    if (!write_text_file(dir + "/manifest.json", core::build_manifest(m))) {
        if (outError) *outError = "could not write manifest";
        return false;
    }
    return true;
}

// restore() is added in Task 6.

} // namespace thomaz

#endif // __SWITCH__
```

- [ ] **Step 3: Add a temporary restore stub so the file compiles on its own**

Add this just above the `// restore() is added in Task 6.` line (it is replaced in Task 6):

```cpp
bool NsSaveService::restore(const core::BackupEntry&, std::uint64_t, std::string* outError) {
    if (outError) *outError = "not implemented";
    return false;
}
```

- [ ] **Step 4: Compile in Docker to verify libnx usage**

Run:
```bash
cp -rn lib/borealis/resources/* resources/
docker run --rm -v "$PWD":/work -w /work devkitpro/devkita64 bash -c \
  'cmake -B build_switch -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON >/dev/null 2>&1; make -C build_switch thomaz.nro -j$(nproc) 2>&1 | tail -15'
```
Expected: `Built target thomaz.nro`. If a libnx symbol mismatches (e.g. `accountProfileGet` arg shape, `ACC_USER_LIST_SIZE`), open `$DEVKITPRO/libnx/include/switch/services/acc.h` inside the container and adjust signatures, then rebuild.

- [ ] **Step 5: Commit**

```bash
git add source/platform/save_service_switch.hpp source/platform/save_service_switch.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(saves): libnx Switch impl — profile scan + backup"
```

---

## Task 6: Switch impl — restore (libnx)

**Files:**
- Modify: `source/platform/save_service_switch.cpp`

- [ ] **Step 1: Replace the restore stub with the real implementation**

In `source/platform/save_service_switch.cpp`, replace the temporary stub from Task 5 Step 3 (and the `// restore() is added in Task 6.` comment) with:

```cpp
bool NsSaveService::restore(const core::BackupEntry& entry, std::uint64_t title_id,
                            std::string* outError) {
    accountInitialize(AccountServiceType_System);

    std::vector<std::string> done;
    std::vector<std::string> skipped;

    for (const std::string& profileHex : entry.profiles) {
        AccountUid uid;
        std::sscanf(profileHex.c_str(), "%016lx%016lx",
                    (unsigned long*)&uid.uid[0], (unsigned long*)&uid.uid[1]);

        // Mounting only succeeds for a profile that still exists and has a save
        // slot for this title. If not, skip it and report.
        if (R_FAILED(fsdevMountSaveData(kMount, title_id, uid))) {
            skipped.push_back(profileHex);
            continue;
        }

        std::string mountRoot = std::string(kMount) + ":/";
        std::string src       = entry.path + "/" + profileHex;

        clear_tree(mountRoot);                 // wipe current save contents
        bool ok = copy_tree(src, mountRoot);   // write backup files back in
        if (ok && R_SUCCEEDED(fsdevCommitDevice(kMount))) // commit, or it is discarded
            done.push_back(profileHex);
        else
            skipped.push_back(profileHex);

        fsdevUnmountDevice(kMount);
    }
    accountExit();

    if (done.empty()) {
        if (outError) *outError = "restore failed (no profiles restored)";
        return false;
    }
    if (!skipped.empty() && outError)
        *outError = "restored " + std::to_string(done.size()) +
                    " profile(s); skipped " + std::to_string(skipped.size());
    return true;
}
```

- [ ] **Step 2: Compile in Docker**

Run:
```bash
docker run --rm -v "$PWD":/work -w /work devkitpro/devkita64 bash -c \
  'make -C build_switch thomaz.nro -j$(nproc) 2>&1 | tail -15'
```
Expected: `Built target thomaz.nro`.

- [ ] **Step 3: Commit**

```bash
git add source/platform/save_service_switch.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(saves): libnx restore — mount, clear, copy back, commit"
```

---

## Task 7: i18n keys

**Files:**
- Modify: the per-language i18n files under `resources/i18n/<lang>/`

- [ ] **Step 1: Find the i18n file name and the existing key style**

Run: `ls resources/i18n/en-US/ && grep -n '"saves"' -r resources/i18n/en-US/ ; grep -rn 'home/saves\|module/settings/title' resources/i18n/en-US/`
Expected: shows the JSON file(s) and how nested keys like `thomaz/home/saves` are structured (nested objects under `thomaz` → `home` → `saves`).

- [ ] **Step 2: Add the Save Manager keys to en-US**

In the en-US i18n JSON, under the existing `thomaz` object, add a `saves` block (merge with any existing `home.saves` label; these are the *new* screen strings). Use the same nesting style you observed in Step 1:

```json
"saves": {
  "title": "Save Manager",
  "subtitle": "Back up and restore your game saves",
  "last_backup": "Last backup: {{when}}",
  "never": "Never backed up",
  "action_backup": "Back up now",
  "action_restore": "Restore",
  "history": "Backups",
  "confirm_restore_title": "Restore this backup?",
  "confirm_restore_body": "This overwrites the current save for this game.",
  "backing_up": "Backing up…",
  "restoring": "Restoring…",
  "backup_ok": "Backup complete",
  "backup_none": "No save data to back up",
  "backup_fail": "Backup failed",
  "restore_ok": "Restore complete",
  "restore_fail": "Restore failed",
  "empty": "No games installed"
}
```

- [ ] **Step 3: Mirror the keys into the other four languages**

Add the same `saves` block to `pt-BR`, `fr`, `ru`, `zh-Hans`. pt-BR values:

```json
"saves": {
  "title": "Gerenciador de Saves",
  "subtitle": "Faça backup e restaure os saves dos seus jogos",
  "last_backup": "Último backup: {{when}}",
  "never": "Nunca foi feito backup",
  "action_backup": "Fazer backup agora",
  "action_restore": "Restaurar",
  "history": "Backups",
  "confirm_restore_title": "Restaurar este backup?",
  "confirm_restore_body": "Isso sobrescreve o save atual deste jogo.",
  "backing_up": "Fazendo backup…",
  "restoring": "Restaurando…",
  "backup_ok": "Backup concluído",
  "backup_none": "Nenhum save para backup",
  "backup_fail": "Falha no backup",
  "restore_ok": "Restauração concluída",
  "restore_fail": "Falha na restauração",
  "empty": "Nenhum jogo instalado"
}
```

For `fr`, `ru`, `zh-Hans`: use the English values as a starting point (translate the visible strings; keep `{{when}}` intact). It is acceptable to copy the en-US block verbatim into these three if a translation is not readily available — the keys must exist so lookups don't fail.

- [ ] **Step 4: Verify JSON validity**

Run: `for f in resources/i18n/*/$(ls resources/i18n/en-US/); do python3 -m json.tool "$f" >/dev/null && echo "OK $f" || echo "BAD $f"; done`
Expected: `OK` for every file.

- [ ] **Step 5: Commit**

```bash
git add resources/i18n
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(saves): i18n strings for Save Manager (5 languages)"
```

---

## Task 8: Save Manager activity (Screen 1)

**Files:**
- Create: `source/app/save_manager_activity.hpp`
- Create: `source/app/save_manager_activity.cpp`
- Create: `resources/xml/activity/save_manager.xml`

- [ ] **Step 1: Write the XML layout**

Create `resources/xml/activity/save_manager.xml` (mirrors `game_list.xml`: a titled frame, a spinner shown while loading, an empty label, and a scrolling list box):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<brls:AppletFrame title="@i18n/thomaz/saves/title" iconInterpolation="linear">
    <brls:Box axis="column" grow="1.0"
              paddingTop="20" paddingBottom="20" paddingLeft="40" paddingRight="40">

        <brls:Label text="@i18n/thomaz/saves/subtitle"
                    fontSize="16" textColor="@theme/thomaz/text_dim" marginBottom="16"/>

        <brls:Spinner id="spinner" width="48" height="48"
                      marginTop="40" alignSelf="center"/>

        <brls:Label id="emptyLabel" text="@i18n/thomaz/saves/empty"
                    fontSize="18" textColor="@theme/thomaz/text_dim"
                    marginTop="40" alignSelf="center"
                    visibility="gone"/>

        <brls:ScrollingFrame grow="1.0">
            <brls:Box id="saveListBox" axis="column" grow="1.0"/>
        </brls:ScrollingFrame>
    </brls:Box>
</brls:AppletFrame>
```

If `<brls:Spinner>` is not the tag used elsewhere, copy the exact spinner element from `resources/xml/activity/game_list.xml`.

- [ ] **Step 2: Write the activity header**

Create `source/app/save_manager_activity.hpp`:

```cpp
#pragma once

#include <borealis.hpp>
#include <memory>
#include <vector>

#include "platform/save_service.hpp"
#include "platform/title.hpp"

namespace thomaz {

// Screen 1: lists installed games with their last-backup date; tapping a row
// opens the detail screen for that game.
class SaveManagerActivity : public brls::Activity
{
  public:
    SaveManagerActivity(ITitleService* titleService, ISaveService* saveService);
    ~SaveManagerActivity() override;

    CONTENT_FROM_XML_RES("activity/save_manager.xml");

    void onContentAvailable() override;

  private:
    void populate(const std::vector<InstalledTitle>& titles);

    ITitleService* titleService;
    ISaveService* saveService;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
};

} // namespace thomaz
```

- [ ] **Step 3: Write the activity implementation**

Create `source/app/save_manager_activity.cpp`:

```cpp
#include "app/save_manager_activity.hpp"
#include "app/save_detail_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>

#include "core/backup_store.hpp"

using namespace brls::literals;

namespace thomaz {

SaveManagerActivity::SaveManagerActivity(ITitleService* titleService, ISaveService* saveService)
    : titleService(titleService), saveService(saveService)
{
}

SaveManagerActivity::~SaveManagerActivity()
{
    *this->alive = false;
}

void SaveManagerActivity::onContentAvailable()
{
    ITitleService* svc = this->titleService;
    auto alive         = this->alive;

    brls::async([this, svc, alive]() {
        auto titles = svc->listInstalled();
        brls::sync([this, alive, titles]() {
            if (!alive->load())
                return;
            this->populate(titles);
        });
    });
}

void SaveManagerActivity::populate(const std::vector<InstalledTitle>& titles)
{
    brls::Box* listBox      = (brls::Box*)this->getView("saveListBox");
    brls::Label* emptyLabel = (brls::Label*)this->getView("emptyLabel");
    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::GONE);

    if (titles.empty()) {
        if (emptyLabel) emptyLabel->setVisibility(brls::Visibility::VISIBLE);
        return;
    }
    if (!listBox)
        return;

    std::string root = core::saves_root();
    ISaveService* save = this->saveService;

    for (const auto& title : titles) {
        brls::Box* row = new brls::Box(brls::Axis::ROW);
        row->setHeight(64.0f);
        row->setFocusable(true);
        row->setMarginBottom(4.0f);
        row->setPadding(12.0f, 20.0f, 12.0f, 20.0f);
        row->setBackgroundColor(nvgRGB(0x1A, 0x1C, 0x23));
        row->setCornerRadius(12.0f);
        row->setAlignItems(brls::AlignItems::CENTER);

        if (!title.icon.empty()) {
            brls::Image* icon = new brls::Image();
            icon->setWidth(48.0f); icon->setHeight(48.0f);
            icon->setCornerRadius(8.0f);
            icon->setScalingType(brls::ImageScalingType::FILL);
            icon->setMarginRight(16.0f);
            icon->setImageFromMem(title.icon.data(), (int)title.icon.size());
            row->addView(icon);
        } else {
            brls::Box* ph = new brls::Box();
            ph->setWidth(48.0f); ph->setHeight(48.0f);
            ph->setCornerRadius(8.0f); ph->setMarginRight(16.0f);
            ph->setBackgroundColor(nvgRGB(0x22, 0x24, 0x2D));
            row->addView(ph);
        }

        brls::Box* textCol = new brls::Box(brls::Axis::COLUMN);
        textCol->setGrow(1.0f);
        brls::Label* nameLabel = new brls::Label();
        nameLabel->setText(title.name);
        nameLabel->setFontSize(18.0f);
        textCol->addView(nameLabel);

        brls::Label* dateLabel = new brls::Label();
        auto last = core::last_backup_timestamp(root, title.title_id);
        if (last)
            dateLabel->setText(brls::getStr("thomaz/saves/last_backup",
                                            {{"when", core::format_timestamp_label(*last)}}));
        else
            dateLabel->setText("thomaz/saves/never"_i18n);
        dateLabel->setFontSize(13.0f);
        dateLabel->setTextColor(nvgRGB(0x8A, 0x8C, 0x99));
        textCol->addView(dateLabel);
        row->addView(textCol);

        InstalledTitle rowTitle = title;
        row->registerClickAction([rowTitle, save](brls::View*) {
            brls::Application::pushActivity(new SaveDetailActivity(rowTitle, save));
            return true;
        });
        row->addGestureRecognizer(new brls::TapGestureRecognizer(row));
        listBox->addView(row);
    }
}

} // namespace thomaz
```

Note on the `last_backup` string: it uses `{{when}}` interpolation. If `brls::getStr` with a substitution map is not the helper used in this codebase, check how other interpolated i18n strings are built (grep `getStr` under `lib/borealis`) and match that call; otherwise concatenate manually: `"thomaz/saves/last_backup"_i18n` won't interpolate on its own.

- [ ] **Step 4: Verify the interpolation helper exists**

Run: `grep -rn "getStr" lib/borealis/library/include | head`
Expected: shows `getStr(string, map)` (fmt-style). If it does NOT exist, change the `dateLabel` line to build the text by hand, e.g.:
```cpp
dateLabel->setText("thomaz/saves/last_backup"_i18n + std::string(" ") +
                   core::format_timestamp_label(*last));
```
and simplify the en/pt strings to a bare label "Last backup:" / "Último backup:".

- [ ] **Step 5: Build desktop (will fail to link until Task 9 provides SaveDetailActivity)**

Skip building standalone — `SaveDetailActivity` is created in Task 9. Proceed to commit the two files; the desktop build is run after Task 9.

- [ ] **Step 6: Commit**

```bash
git add source/app/save_manager_activity.hpp source/app/save_manager_activity.cpp resources/xml/activity/save_manager.xml
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(saves): Save Manager screen (game list + last-backup date)"
```

---

## Task 9: Save detail activity (Screen 2)

**Files:**
- Create: `source/app/save_detail_activity.hpp`
- Create: `source/app/save_detail_activity.cpp`
- Create: `resources/xml/activity/save_detail.xml`

- [ ] **Step 1: Write the XML layout**

Create `resources/xml/activity/save_detail.xml`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<brls:AppletFrame title="@i18n/thomaz/saves/title" iconInterpolation="linear">
    <brls:Box axis="column" grow="1.0"
              paddingTop="20" paddingBottom="20" paddingLeft="40" paddingRight="40">

        <brls:Label id="gameName" text="" fontSize="26" textColor="@theme/thomaz/text"/>
        <brls:Label id="lastBackup" text="" fontSize="15"
                    textColor="@theme/thomaz/text_dim" marginTop="4" marginBottom="18"/>

        <brls:Box id="backupButton" axis="row" alignItems="center" justifyContent="center"
                  height="56" cornerRadius="14" focusable="true"
                  backgroundColor="@theme/thomaz/tile_saves" marginBottom="20">
            <brls:Label text="@i18n/thomaz/saves/action_backup"
                        fontSize="18" textColor="#FFFFFF"/>
        </brls:Box>

        <brls:Label text="@i18n/thomaz/saves/history"
                    fontSize="15" textColor="@theme/thomaz/text_dim" marginBottom="8"/>
        <brls:Spinner id="spinner" width="36" height="36" visibility="gone" alignSelf="center"/>
        <brls:ScrollingFrame grow="1.0">
            <brls:Box id="historyBox" axis="column" grow="1.0"/>
        </brls:ScrollingFrame>
    </brls:Box>
</brls:AppletFrame>
```

- [ ] **Step 2: Write the header**

Create `source/app/save_detail_activity.hpp`:

```cpp
#pragma once

#include <borealis.hpp>
#include <atomic>
#include <memory>

#include "platform/save_service.hpp"
#include "platform/title.hpp"

namespace thomaz {

// Screen 2: shows one game's last-backup date, a "back up now" button, and the
// list of existing backups, each restorable.
class SaveDetailActivity : public brls::Activity
{
  public:
    SaveDetailActivity(InstalledTitle title, ISaveService* saveService);
    ~SaveDetailActivity() override;

    CONTENT_FROM_XML_RES("activity/save_detail.xml");

    void onContentAvailable() override;

  private:
    void refreshHistory();   // rebuild the history list + last-backup label
    void doBackup();
    void doRestore(const core::BackupEntry& entry);

    InstalledTitle title;
    ISaveService* saveService;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
};

} // namespace thomaz
```

- [ ] **Step 3: Write the implementation**

Create `source/app/save_detail_activity.cpp`:

```cpp
#include "app/save_detail_activity.hpp"

#include <borealis.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>

#include "core/backup_store.hpp"

using namespace brls::literals;

namespace thomaz {

SaveDetailActivity::SaveDetailActivity(InstalledTitle title, ISaveService* saveService)
    : title(std::move(title)), saveService(saveService)
{
}

SaveDetailActivity::~SaveDetailActivity()
{
    *this->alive = false;
}

void SaveDetailActivity::onContentAvailable()
{
    if (auto* name = (brls::Label*)this->getView("gameName"))
        name->setText(this->title.name);

    if (auto* btn = this->getView("backupButton")) {
        btn->registerClickAction([this](brls::View*) { this->doBackup(); return true; });
        btn->addGestureRecognizer(new brls::TapGestureRecognizer(btn));
    }
    this->refreshHistory();
}

void SaveDetailActivity::refreshHistory()
{
    std::string root = core::saves_root();

    if (auto* lbl = (brls::Label*)this->getView("lastBackup")) {
        auto last = core::last_backup_timestamp(root, this->title.title_id);
        lbl->setText(last
            ? ("thomaz/saves/last_backup"_i18n + std::string(" ") + core::format_timestamp_label(*last))
            : "thomaz/saves/never"_i18n);
    }

    brls::Box* box = (brls::Box*)this->getView("historyBox");
    if (!box)
        return;
    box->clearViews();

    auto entries = core::list_backups(root, this->title.title_id);
    for (const auto& entry : entries) {
        brls::Box* row = new brls::Box(brls::Axis::ROW);
        row->setHeight(52.0f);
        row->setFocusable(true);
        row->setMarginBottom(4.0f);
        row->setPadding(8.0f, 16.0f, 8.0f, 16.0f);
        row->setCornerRadius(10.0f);
        row->setAlignItems(brls::AlignItems::CENTER);
        row->setBackgroundColor(nvgRGB(0x1A, 0x1C, 0x23));

        brls::Label* ts = new brls::Label();
        ts->setText(core::format_timestamp_label(entry.timestamp));
        ts->setFontSize(16.0f);
        ts->setGrow(1.0f);
        row->addView(ts);

        brls::Label* action = new brls::Label();
        action->setText("thomaz/saves/action_restore"_i18n);
        action->setFontSize(14.0f);
        action->setTextColor(nvgRGB(0x92, 0x77, 0xFF));
        row->addView(action);

        core::BackupEntry captured = entry;
        row->registerClickAction([this, captured](brls::View*) {
            this->doRestore(captured);
            return true;
        });
        row->addGestureRecognizer(new brls::TapGestureRecognizer(row));
        box->addView(row);
    }
}

void SaveDetailActivity::doBackup()
{
    if (auto* spinner = this->getView("spinner"))
        spinner->setVisibility(brls::Visibility::VISIBLE);

    ISaveService* svc = this->saveService;
    InstalledTitle t  = this->title;
    auto alive        = this->alive;

    brls::async([this, svc, t, alive]() {
        std::string err;
        bool ok = svc->backup(t, &err);
        brls::sync([this, alive, ok, err]() {
            if (!alive->load())
                return;
            if (auto* spinner = this->getView("spinner"))
                spinner->setVisibility(brls::Visibility::GONE);
            brls::Application::notify(ok ? "thomaz/saves/backup_ok"_i18n
                                         : ("thomaz/saves/backup_fail"_i18n + std::string(": ") + err));
            if (ok)
                this->refreshHistory();
        });
    });
}

void SaveDetailActivity::doRestore(const core::BackupEntry& entry)
{
    // Destructive — confirm first.
    brls::Application::crash; // placeholder removed below
    auto doIt = [this, entry]() {
        if (auto* spinner = this->getView("spinner"))
            spinner->setVisibility(brls::Visibility::VISIBLE);
        ISaveService* svc       = this->saveService;
        std::uint64_t tid       = this->title.title_id;
        core::BackupEntry e     = entry;
        auto alive              = this->alive;
        brls::async([this, svc, e, tid, alive]() {
            std::string err;
            bool ok = svc->restore(e, tid, &err);
            brls::sync([this, alive, ok, err]() {
                if (!alive->load())
                    return;
                if (auto* spinner = this->getView("spinner"))
                    spinner->setVisibility(brls::Visibility::GONE);
                brls::Application::notify(ok ? "thomaz/saves/restore_ok"_i18n
                                             : ("thomaz/saves/restore_fail"_i18n + std::string(": ") + err));
            });
        });
    };

    brls::Dialog* dialog = new brls::Dialog("thomaz/saves/confirm_restore_body"_i18n);
    dialog->addButton("thomaz/saves/action_restore"_i18n, [doIt]() { doIt(); });
    dialog->addButton("brls/hints/back"_i18n, []() {});
    dialog->open();
}

} // namespace thomaz
```

- [ ] **Step 4: Fix the confirm dialog — remove the placeholder line**

The line `brls::Application::crash; // placeholder removed below` is intentionally invalid pseudo-code to force you to look here. DELETE that line. The real confirmation is the `brls::Dialog` at the bottom of `doRestore`. Verify the Dialog API against the codebase:

Run: `grep -rn "class Dialog\|addButton\|Dialog(" lib/borealis/library/include lib/borealis/library/lib | head`
Expected: confirms `Dialog(std::string)` ctor + `addButton(std::string, callback)` + `open()`. If the signature differs, adapt the four dialog lines to the real API. Also confirm `brls::Application::notify` exists (`grep -rn "void notify" lib/borealis/library/include`); if not, replace the two `notify(...)` calls with the codebase's toast/notification call.

- [ ] **Step 5: Build desktop**

Run: `make -C build_desktop -j$(nproc) 2>&1 | tail -15`
Expected: still won't link into the app until Task 10/11 wire it, but the new translation units must COMPILE. If `SaveManagerActivity`/`SaveDetailActivity` compile cleanly, proceed. Fix any compile errors surfaced here (API mismatches from Step 4).

- [ ] **Step 6: Commit**

```bash
git add source/app/save_detail_activity.hpp source/app/save_detail_activity.cpp resources/xml/activity/save_detail.xml
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(saves): save detail screen — backup button + restore history"
```

---

## Task 10: Wire the home tile

**Files:**
- Modify: `resources/xml/activity/home.xml:69-87`
- Modify: `source/app/home_activity.hpp`
- Modify: `source/app/home_activity.cpp`

- [ ] **Step 1: Make the Save Manager tile focusable + id'd**

In `resources/xml/activity/home.xml`, the "Save Manager (coming soon)" Box (currently lines ~70-87) — replace its opening tag and remove the "coming soon"/lock children. Change:

```xml
                <!-- Save Manager (coming soon) -->
                <brls:Box axis="row" alignItems="center" grow="1.0" marginBottom="14"
                          paddingLeft="22" paddingRight="22" cornerRadius="16"
                          backgroundColor="@theme/thomaz/tile_saves">
```

to:

```xml
                <!-- Save Manager (active) -->
                <brls:Box id="savesCard"
                          axis="row" alignItems="center" grow="1.0" marginBottom="14"
                          paddingLeft="22" paddingRight="22" cornerRadius="16"
                          backgroundColor="@theme/thomaz/tile_saves"
                          focusable="true" highlightCornerRadius="16"
                          hideHighlightBackground="true">
```

Then, within that Box, delete the "coming soon" sublabel and the lock label so it matches the active Settings tile style:

- Remove:
  ```xml
                        <brls:Label text="@i18n/thomaz/common/coming_soon"
                                    fontSize="11" textColor="#FFFFFFB3" marginTop="3"/>
  ```
- Remove the trailing lock label:
  ```xml
                    <brls:Label text="@i18n/thomaz/icon/lock"
                                fontSize="20" textColor="#FFFFFFB3"/>
  ```

(Leave the Mods tile unchanged — it stays "coming soon".)

- [ ] **Step 2: Update the HomeActivity constructor signature**

In `source/app/home_activity.hpp`:
- Add include: `#include "platform/save_service.hpp"`
- Change the constructor to:
  ```cpp
  HomeActivity(ITitleService* titleService, IHttpClient* http, ISaveService* saveService);
  ```
- Add the member:
  ```cpp
  ISaveService* saveService;
  ```

- [ ] **Step 3: Update the HomeActivity implementation**

In `source/app/home_activity.cpp`:
- Add include: `#include "app/save_manager_activity.hpp"`
- Change the constructor definition to store `saveService`:
  ```cpp
  HomeActivity::HomeActivity(ITitleService* titleService, IHttpClient* http, ISaveService* saveService)
      : titleService(titleService), http(http), saveService(saveService)
  {
  }
  ```
- At the end of `onContentAvailable()`, before the closing brace, add:
  ```cpp
      if (brls::View* saves = this->getView("savesCard")) {
          saves->registerClickAction([this](brls::View*) {
              brls::Application::pushActivity(
                  new SaveManagerActivity(this->titleService, this->saveService));
              return true;
          });
          saves->addGestureRecognizer(new brls::TapGestureRecognizer(saves));
      }
  ```

- [ ] **Step 4: Commit (build happens in Task 11 once main.cpp constructs the service)**

```bash
git add resources/xml/activity/home.xml source/app/home_activity.hpp source/app/home_activity.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(saves): activate Save Manager home tile -> SaveManagerActivity"
```

---

## Task 11: Construct the service in main + desktop end-to-end

**Files:**
- Modify: `source/main.cpp`

- [ ] **Step 1: Include both impls and the interface**

In `source/main.cpp`, near the existing platform includes (around lines 7-12), add inside the `#ifdef __SWITCH__` / `#else` branches:

```cpp
#ifdef __SWITCH__
#include <switch.h>
#include "platform/title_service_switch.hpp"
#include "platform/save_service_switch.hpp"
#else
#include "platform/title_service_fake.hpp"
#include "platform/save_service_fake.hpp"
#endif
```

- [ ] **Step 2: Construct the save service alongside the title service**

After the `titleService` block (around lines 60-69), add:

```cpp
    // Save backup/restore service for the current platform.
#ifdef __SWITCH__
    auto saveService = std::make_unique<thomaz::NsSaveService>();
#else
    auto saveService = std::make_unique<thomaz::FakeSaveService>();
#endif
```

- [ ] **Step 3: Pass it into HomeActivity**

Change the `pushActivity` line (currently line 77) to:

```cpp
    brls::Application::pushActivity(
        new thomaz::HomeActivity(titleService.get(), httpClient.get(), saveService.get()));
```

- [ ] **Step 4: Build the desktop app**

Run: `make -C build_desktop -j$(nproc) 2>&1 | tail -15`
Expected: links cleanly into `build_desktop/thomaz`.

- [ ] **Step 5: Smoke-run + exercise the flow on desktop**

Run: `rm -rf thomaz-saves && timeout 4 ./build_desktop/thomaz; echo "exit=$?"`
Expected: `exit=124` (ran the full 4s without crashing). The Save Manager tile is reachable from home; using it triggers `FakeSaveService` which writes under `./thomaz-saves/`.

Then verify the fake backup wrote a valid tree:
Run: `find thomaz-saves -maxdepth 3 -type f 2>/dev/null; echo "---"; cat thomaz-saves/*/*/manifest.json 2>/dev/null | head`
Expected: after navigating + backing up at least once, a `manifest.json` exists. (If the run is non-interactive and nothing was clicked, this may be empty — that's fine; the unit tests already cover the store, and on-console testing covers the real path.)

- [ ] **Step 6: Run the unit tests once more (nothing regressed)**

Run: `cd tests && make test && cd .. && rm -rf test-saves-tmp`
Expected: all tests PASS.

- [ ] **Step 7: Commit**

```bash
git add source/main.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' \
  commit -m "feat(saves): construct ISaveService in main + wire into home"
```

---

## Task 12: Switch build + on-console verification notes

**Files:** none (build + manual verification)

- [ ] **Step 1: Merge Borealis resources and build the .nro in Docker**

Run:
```bash
cp -rn lib/borealis/resources/* resources/
docker run --rm -v "$PWD":/work -w /work devkitpro/devkita64 bash -c \
  'cmake -B build_switch -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON >/dev/null 2>&1; make -C build_switch thomaz.nro -j$(nproc) 2>&1 | tail -20'
```
Expected: `Built target thomaz.nro`, output ~4-5 MB at `build_switch/thomaz.nro`.

- [ ] **Step 2: Deliver the .nro to the user for on-console testing**

Copy it out and hand it over (do NOT push/tag):
```bash
cp build_switch/thomaz.nro /tmp/thomaz-saves.nro
```
Then send `/tmp/thomaz-saves.nro` to the user with a note describing the on-console checklist below.

**On-console checklist (user-run, manual — cannot be automated here):**
1. Home → Save Manager opens; installed games list with "Nunca" for games never backed up.
2. Open a game with a save → "Fazer backup agora" → spinner → success toast; "Último backup" date appears.
3. Confirm on SD: `/switch/thomaz/saves/<titleid>/<timestamp>/<uid>/...` files + `manifest.json` exist.
4. A game with no save reports "Nenhum save para backup", leaves no empty folder.
5. Restore: pick a backup → confirm dialog → spinner → success; launch the game and verify the save state matches the backup.

- [ ] **Step 3: Final state commit (if any resource merge left tracked changes)**

```bash
git status --short
# Only commit source/resource changes that are intended; the borealis resource
# copy targets gitignored/already-tracked files — do not commit vendored copies.
```

---

## Self-Review

**Spec coverage:**
- Read real saves, all profiles auto → Tasks 5 (`all_profiles` + mount-probe) ✓
- Folder-tree backup on SD + manifest.json → Tasks 1, 2, 5 ✓
- Restore from SD with confirmation + commit → Tasks 6, 9 (Dialog) ✓
- Last-backup date in UI → Tasks 3, 8 ✓
- Two screens (list → detail) → Tasks 8, 9 ✓
- Home tile activation → Task 10 ✓
- Platform interface + Switch/fake + pure core → Tasks 1-6 ✓
- Error handling (no save, partial cleanup, missing profile on restore) → Task 5 (`savedProfiles.empty()` cleanup), Task 6 (`skipped`) ✓
- Unit tests for core → Tasks 1-3 ✓
- i18n 5 languages → Task 7 ✓
- No login / no cloud / no zip → out of scope, not implemented ✓

**Placeholder scan:** The only intentional "placeholder" is the invalid `brls::Application::crash;` line in Task 9 Step 3, which Step 4 explicitly instructs to delete and replace with the verified Dialog API — this is a deliberate attention checkpoint, not an unfinished step. No TBD/TODO remain.

**Type consistency:** `SaveProfile`/`BackupEntry`/`ManifestInfo` (in `core::`) are used identically across core, fake, switch, and activities. `ISaveService` methods (`profilesWithSave`, `backup`, `restore`) match between interface (Task 4), fake (Task 4), switch (Tasks 5-6), and callers (Tasks 8-9). `saves_root()`/`backup_dir()`/`list_backups()`/`last_backup_timestamp()`/`format_timestamp_label()` signatures are consistent between definition (Tasks 2-3) and use (Tasks 5, 8, 9).

**Known verification points flagged inline (API shapes to confirm against the live tree, since they can't be checked offline):** `brls::getStr` interpolation (Task 8 Step 4), `brls::Dialog`/`Application::notify` (Task 9 Step 4), `<brls:Spinner>` tag (Task 8 Step 1), and libnx `accountListAllUsers`/`fsdevMountSaveData`/`accountProfileGet`/`fsdevCommitDevice` signatures (Task 5 Step 4 via Docker compile).
