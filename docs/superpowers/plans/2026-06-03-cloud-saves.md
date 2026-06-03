# Cloud Saves Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add manual cloud upload/download of game saves to the Save Detail screen, talking to the real `thomaz-api` (`GET/PUT /saves/:titleId`), with cloud status on screen-open and `revision`-based conflict resolution.

**Architecture:** Pure, testable core (blob packaging, sync-state codec, sync decision logic, JSON/base64 parsing) under `source/core/saves/`. A platform `ICloudSaveClient` with a real `HttpCloudSaveClient` (over the existing `IHttpClient`) and an in-memory `FakeCloudSaveClient` for tests. `ISaveService` gains two methods (read active save → blob; write blob → local backup). The Save Detail activity gets a "Cloud" section gated on the feed's existing login session.

**Tech Stack:** C++17, Borealis UI, CMake (app) + Makefile/doctest (tests), nlohmann/json, libcurl (`CurlHttpClient`), libnx (`fsdevMountSaveData`) on Switch.

**Spec:** `docs/superpowers/specs/2026-06-03-cloud-saves-design.md`

---

## File Structure

**Create (core, pure, unit-tested):**
- `source/core/saves/save_package.{hpp,cpp}` — pack/unpack a save blob (length-prefixed binary).
- `source/core/saves/save_sync_state.{hpp,cpp}` — `titleId → revision` text codec.
- `source/core/saves/save_sync.{hpp,cpp}` — `classify()` + `plan_push()` decision logic.
- `source/core/saves/cloud_save_json.{hpp,cpp}` — parse slot meta/data + base64 decode + error messages.

**Create (platform):**
- `source/platform/saves/cloud_save_client.hpp` — `ICloudSaveClient` interface + result structs.
- `source/platform/saves/http_cloud_save_client.{hpp,cpp}` — real impl over `IHttpClient`.
- `source/platform/saves/fake_cloud_save_client.{hpp,cpp}` — in-memory fake for tests.
- `source/platform/saves/save_backup_io.{hpp,cpp}` — shared "write package as local backup" helper.
- `source/platform/saves/sync_store.{hpp,cpp}` — read/write the per-title synced revision file.

**Create (tests):**
- `tests/test_save_package.cpp`, `tests/test_save_sync_state.cpp`, `tests/test_save_sync.cpp`, `tests/test_cloud_save_json.cpp`, `tests/test_fake_cloud_save_client.cpp`, `tests/test_http_cloud_save_client.cpp`.

**Modify:**
- `source/platform/save_service.hpp` — add `packageActiveSave` + `importPackageAsBackup`.
- `source/platform/save_service_fake.{hpp,cpp}` — implement them (desktop).
- `source/platform/save_service_switch.{hpp,cpp}` — implement them (Switch).
- `source/app/save_detail_activity.{hpp,cpp}` — Cloud section + flows.
- `source/app/save_manager_activity.{hpp,cpp}` — thread `ICloudSaveClient*` + `IFeedClient*`.
- `source/app/home_activity.{hpp,cpp}` — thread `ICloudSaveClient*`.
- `source/main.cpp` — construct `HttpCloudSaveClient`, inject it.
- `resources/xml/activity/save_detail.xml` — Cloud section views.
- `resources/i18n/en-US/thomaz.json`, `resources/i18n/pt-BR/thomaz.json` — cloud strings.
- `tests/Makefile` — add new sources to `SRCS`.

**Conventions (from the existing codebase):**
- Tests: doctest; `tests/test_main.cpp` already provides `main` — never redefine it. Run with `cd tests && make test`.
- Desktop build: `./scripts/build-desktop.sh` (reconfigures CMake, which re-globs `source/*.cpp`). Smoke run: `timeout 8 ./build_desktop/thomaz` — **exit 124 means healthy** (still running).
- `titleId` is sent to the API as a 16-char lowercase hex string (`%016llx`), mirroring `HttpFeedClient`.
- Binary file writes go through `write_text_file(path, std::string)` from `platform/cheat_store.hpp` (it writes raw bytes; build the string from the byte buffer).
- Git identity for every commit: `git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit ...`, and end each message with a blank line then `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.

---

## Task 1: save_package — pack/unpack the save blob

**Files:**
- Create: `source/core/saves/save_package.hpp`, `source/core/saves/save_package.cpp`
- Test: `tests/test_save_package.cpp`
- Modify: `tests/Makefile`

- [ ] **Step 1: Add the new core sources to the test Makefile**

In `tests/Makefile`, change the `SRCS` line to append the saves core glob and the two cloud-client sources (added in later tasks; harmless to list now since the files will exist before they are compiled together — if a referenced file does not yet exist, defer this edit to the task that creates it). For this task, only add the saves core glob:

```make
SRCS     := $(wildcard *.cpp) $(wildcard ../source/core/*.cpp) $(wildcard ../source/core/feed/*.cpp) $(wildcard ../source/core/saves/*.cpp) ../source/platform/cheat_store.cpp ../source/platform/feed/http_feed_client.cpp ../source/platform/app_settings.cpp
```

- [ ] **Step 2: Write the failing test**

`tests/test_save_package.cpp`:

```cpp
#include "doctest.h"
#include "core/saves/save_package.hpp"

using namespace thomaz::core;

static std::vector<std::uint8_t> bytes(const std::string& s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

TEST_CASE("empty package round-trips") {
    SavePackage pkg;
    auto blob = pack_save_package(pkg);
    auto out  = unpack_save_package(blob);
    REQUIRE(out.has_value());
    CHECK(out->files.empty());
}

TEST_CASE("single-profile package round-trips") {
    SavePackage pkg;
    pkg.files.push_back({ "1111/save.dat", bytes("hello world") });
    auto out = unpack_save_package(pack_save_package(pkg));
    REQUIRE(out.has_value());
    REQUIRE(out->files.size() == 1);
    CHECK(out->files[0].path == "1111/save.dat");
    CHECK(out->files[0].bytes == bytes("hello world"));
}

TEST_CASE("multi-profile package preserves order and bytes") {
    SavePackage pkg;
    pkg.files.push_back({ "aaaa/a.bin", bytes("A") });
    pkg.files.push_back({ "bbbb/sub/b.bin", bytes("BB") });
    pkg.files.push_back({ "aaaa/c.bin", {} }); // zero-length file
    auto out = unpack_save_package(pack_save_package(pkg));
    REQUIRE(out.has_value());
    REQUIRE(out->files.size() == 3);
    CHECK(out->files[1].path == "bbbb/sub/b.bin");
    CHECK(out->files[1].bytes == bytes("BB"));
    CHECK(out->files[2].bytes.empty());
}

TEST_CASE("corrupted blob is rejected") {
    CHECK_FALSE(unpack_save_package({}).has_value());                 // empty
    CHECK_FALSE(unpack_save_package(bytes("XXXX")).has_value());      // bad magic
    // valid magic + count=1 but truncated entry:
    std::vector<std::uint8_t> b = { 'T','S','A','V', 1,0,0,0, 5,0,0,0 };
    CHECK_FALSE(unpack_save_package(b).has_value());
}
```

- [ ] **Step 3: Run it to confirm it fails**

Run: `cd tests && make clean && make test`
Expected: compile error — `core/saves/save_package.hpp` not found.

- [ ] **Step 4: Write the header**

`source/core/saves/save_package.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace thomaz::core {

// One file inside a save, keyed by a path relative to the save root. The first
// path segment is the profile's uid_hex (e.g. "1111.../save.dat").
struct SaveFileEntry {
    std::string               path;
    std::vector<std::uint8_t> bytes;
};

// A whole save (all profiles) as an ordered list of files. This is what gets
// serialized into the opaque blob the API stores.
struct SavePackage {
    std::vector<SaveFileEntry> files;
};

// Binary layout (little-endian): magic "TSAV", u32 fileCount, then per file:
// u32 pathLen, path bytes, u32 dataLen, data bytes.
std::vector<std::uint8_t> pack_save_package(const SavePackage& pkg);

// Returns nullopt on any malformed/truncated input.
std::optional<SavePackage> unpack_save_package(const std::vector<std::uint8_t>& blob);

} // namespace thomaz::core
```

- [ ] **Step 5: Write the implementation**

`source/core/saves/save_package.cpp`:

```cpp
#include "core/saves/save_package.hpp"

namespace thomaz::core {

namespace {
void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back((std::uint8_t)(v & 0xFF));
    out.push_back((std::uint8_t)((v >> 8) & 0xFF));
    out.push_back((std::uint8_t)((v >> 16) & 0xFF));
    out.push_back((std::uint8_t)((v >> 24) & 0xFF));
}

// Reads a u32 at `pos`, advancing it. Returns false if out of bounds.
bool read_u32(const std::vector<std::uint8_t>& b, size_t& pos, std::uint32_t& out) {
    if (pos + 4 > b.size()) return false;
    out = (std::uint32_t)b[pos] | ((std::uint32_t)b[pos + 1] << 8) |
          ((std::uint32_t)b[pos + 2] << 16) | ((std::uint32_t)b[pos + 3] << 24);
    pos += 4;
    return true;
}
} // namespace

std::vector<std::uint8_t> pack_save_package(const SavePackage& pkg) {
    std::vector<std::uint8_t> out = { 'T', 'S', 'A', 'V' };
    put_u32(out, (std::uint32_t)pkg.files.size());
    for (const auto& f : pkg.files) {
        put_u32(out, (std::uint32_t)f.path.size());
        out.insert(out.end(), f.path.begin(), f.path.end());
        put_u32(out, (std::uint32_t)f.bytes.size());
        out.insert(out.end(), f.bytes.begin(), f.bytes.end());
    }
    return out;
}

std::optional<SavePackage> unpack_save_package(const std::vector<std::uint8_t>& blob) {
    if (blob.size() < 8) return std::nullopt;
    if (!(blob[0] == 'T' && blob[1] == 'S' && blob[2] == 'A' && blob[3] == 'V'))
        return std::nullopt;

    size_t pos = 4;
    std::uint32_t count = 0;
    if (!read_u32(blob, pos, count)) return std::nullopt;

    SavePackage pkg;
    pkg.files.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t pathLen = 0;
        if (!read_u32(blob, pos, pathLen)) return std::nullopt;
        if (pos + pathLen > blob.size()) return std::nullopt;
        SaveFileEntry e;
        e.path.assign((const char*)&blob[pos], pathLen);
        pos += pathLen;

        std::uint32_t dataLen = 0;
        if (!read_u32(blob, pos, dataLen)) return std::nullopt;
        if (pos + dataLen > blob.size()) return std::nullopt;
        e.bytes.assign(blob.begin() + pos, blob.begin() + pos + dataLen);
        pos += dataLen;

        pkg.files.push_back(std::move(e));
    }
    return pkg;
}

} // namespace thomaz::core
```

- [ ] **Step 6: Run the test**

Run: `cd tests && make test`
Expected: PASS (all `test_save_package` cases green; existing suite unaffected).

- [ ] **Step 7: Commit**

```bash
git add source/core/saves/save_package.hpp source/core/saves/save_package.cpp tests/test_save_package.cpp tests/Makefile
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(saves): save_package pack/unpack for cloud blob

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: save_sync_state — titleId→revision codec

**Files:**
- Create: `source/core/saves/save_sync_state.hpp`, `source/core/saves/save_sync_state.cpp`
- Test: `tests/test_save_sync_state.cpp`

- [ ] **Step 1: Write the failing test**

`tests/test_save_sync_state.cpp`:

```cpp
#include "doctest.h"
#include "core/saves/save_sync_state.hpp"

using namespace thomaz::core;

TEST_CASE("serialize then parse round-trips entries") {
    std::map<std::uint64_t, int> state;
    state[0x0100000000010000ULL] = 3;
    state[0x010000000E5EE000ULL] = 1;
    auto parsed = parse_sync_state(serialize_sync_state(state));
    CHECK(parsed.size() == 2);
    CHECK(synced_revision(parsed, 0x0100000000010000ULL) == 3);
    CHECK(synced_revision(parsed, 0x010000000E5EE000ULL) == 1);
}

TEST_CASE("synced_revision is 0 for an unknown title") {
    std::map<std::uint64_t, int> state;
    CHECK(synced_revision(state, 0xABCDULL) == 0);
}

TEST_CASE("malformed lines are ignored") {
    auto parsed = parse_sync_state("garbage\n0100000000010000 5\n\nonlyonefield\n");
    CHECK(parsed.size() == 1);
    CHECK(synced_revision(parsed, 0x0100000000010000ULL) == 5);
}

TEST_CASE("empty input yields empty state") {
    CHECK(parse_sync_state("").empty());
}
```

- [ ] **Step 2: Run it to confirm it fails**

Run: `cd tests && make test`
Expected: compile error — `core/saves/save_sync_state.hpp` not found.

- [ ] **Step 3: Write the header**

`source/core/saves/save_sync_state.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <map>
#include <string>

namespace thomaz::core {

// In-memory map of the last cloud revision we synced for each title.
// On-disk form: one line per title, "<titleId 16-hex> <revision>".

std::map<std::uint64_t, int> parse_sync_state(const std::string& body);
std::string serialize_sync_state(const std::map<std::uint64_t, int>& state);

// Revision last synced for `titleId`, or 0 if we have never synced it.
int synced_revision(const std::map<std::uint64_t, int>& state, std::uint64_t titleId);

} // namespace thomaz::core
```

- [ ] **Step 4: Write the implementation**

`source/core/saves/save_sync_state.cpp`:

```cpp
#include "core/saves/save_sync_state.hpp"
#include <cstdio>
#include <sstream>

namespace thomaz::core {

std::map<std::uint64_t, int> parse_sync_state(const std::string& body) {
    std::map<std::uint64_t, int> state;
    std::istringstream in(body);
    std::string line;
    while (std::getline(in, line)) {
        unsigned long long id = 0;
        int rev = 0;
        // Expect exactly "<hex> <int>"; sscanf returns the count of matches.
        if (std::sscanf(line.c_str(), "%llx %d", &id, &rev) == 2)
            state[(std::uint64_t)id] = rev;
    }
    return state;
}

std::string serialize_sync_state(const std::map<std::uint64_t, int>& state) {
    std::string out;
    char buf[40];
    for (const auto& [id, rev] : state) {
        std::snprintf(buf, sizeof(buf), "%016llx %d\n",
                      (unsigned long long)id, rev);
        out += buf;
    }
    return out;
}

int synced_revision(const std::map<std::uint64_t, int>& state, std::uint64_t titleId) {
    auto it = state.find(titleId);
    return it == state.end() ? 0 : it->second;
}

} // namespace thomaz::core
```

- [ ] **Step 5: Run the test**

Run: `cd tests && make test`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add source/core/saves/save_sync_state.hpp source/core/saves/save_sync_state.cpp tests/test_save_sync_state.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(saves): save_sync_state codec (titleId -> synced revision)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: save_sync — classify + push plan

**Files:**
- Create: `source/core/saves/save_sync.hpp`, `source/core/saves/save_sync.cpp`
- Test: `tests/test_save_sync.cpp`

- [ ] **Step 1: Write the failing test**

`tests/test_save_sync.cpp`:

```cpp
#include "doctest.h"
#include "core/saves/save_sync.hpp"

using namespace thomaz::core;

TEST_CASE("classify: nothing in the cloud") {
    CHECK(classify(/*cloudExists=*/false, /*cloudRev=*/0, /*syncedRev=*/0) == SyncSituation::NoCloud);
}

TEST_CASE("classify: in sync when revisions match") {
    CHECK(classify(true, 3, 3) == SyncSituation::InSync);
}

TEST_CASE("classify: cloud ahead when its revision is higher than synced") {
    CHECK(classify(true, 5, 3) == SyncSituation::CloudAhead);
}

TEST_CASE("classify: stale-looking lower cloud revision is treated as in sync") {
    // Shouldn't happen, but never falsely flag a conflict.
    CHECK(classify(true, 2, 3) == SyncSituation::InSync);
}

TEST_CASE("plan_push for each situation") {
    PushPlan a = plan_push(SyncSituation::NoCloud, 0);
    CHECK(a.revision == 0);
    CHECK_FALSE(a.isConflict);

    PushPlan b = plan_push(SyncSituation::InSync, 3);
    CHECK(b.revision == 3);
    CHECK_FALSE(b.isConflict);

    PushPlan c = plan_push(SyncSituation::CloudAhead, 5);
    CHECK(c.revision == 5);   // "send mine" overwrites the current cloud revision
    CHECK(c.isConflict);
}
```

- [ ] **Step 2: Run it to confirm it fails**

Run: `cd tests && make test`
Expected: compile error — `core/saves/save_sync.hpp` not found.

- [ ] **Step 3: Write the header**

`source/core/saves/save_sync.hpp`:

```cpp
#pragma once

namespace thomaz::core {

enum class SyncSituation {
    NoCloud,     // the cloud has no slot for this title
    InSync,      // cloud revision == our last synced revision
    CloudAhead,  // cloud revision > our last synced revision (changed elsewhere)
};

SyncSituation classify(bool cloudExists, int cloudRevision, int syncedRevision);

// What revision to PUT and whether this is a conflict the UI must confirm.
struct PushPlan {
    int  revision;    // revision to send with the PUT (server expects current)
    bool isConflict;  // true => ask the user before overwriting the cloud
};

PushPlan plan_push(SyncSituation situation, int cloudRevision);

} // namespace thomaz::core
```

- [ ] **Step 4: Write the implementation**

`source/core/saves/save_sync.cpp`:

```cpp
#include "core/saves/save_sync.hpp"

namespace thomaz::core {

SyncSituation classify(bool cloudExists, int cloudRevision, int syncedRevision) {
    if (!cloudExists) return SyncSituation::NoCloud;
    if (cloudRevision > syncedRevision) return SyncSituation::CloudAhead;
    return SyncSituation::InSync;
}

PushPlan plan_push(SyncSituation situation, int cloudRevision) {
    switch (situation) {
        case SyncSituation::NoCloud:    return PushPlan{ 0, false };
        case SyncSituation::CloudAhead: return PushPlan{ cloudRevision, true };
        case SyncSituation::InSync:
        default:                        return PushPlan{ cloudRevision, false };
    }
}

} // namespace thomaz::core
```

- [ ] **Step 5: Run the test**

Run: `cd tests && make test`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add source/core/saves/save_sync.hpp source/core/saves/save_sync.cpp tests/test_save_sync.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(saves): save_sync classify + push plan

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: cloud_save_json — parse API responses + base64

**Files:**
- Create: `source/core/saves/cloud_save_json.hpp`, `source/core/saves/cloud_save_json.cpp`
- Test: `tests/test_cloud_save_json.cpp`

The API shapes (from `api/src/routes/saves.ts`):
- `GET /saves/:titleId` → `{ "slot": { "titleId": "...", "label": "...", "revision": N, "updatedAt": <epoch s> } }`; `404` → `{ "error": "save_not_found" }`.
- `GET /saves/:titleId?includeData=1` → same `slot` plus `"data": "<base64>"`.
- `PUT /saves/:titleId` → `{ "ok": true, "slot": { ... "revision": N } }`; `409` → `{ "error": "revision_conflict" }`; `413` → `{ "error": "save_too_large" }`.

- [ ] **Step 1: Write the failing test**

`tests/test_cloud_save_json.cpp`:

```cpp
#include "doctest.h"
#include "core/saves/cloud_save_json.hpp"

using namespace thomaz::core;

TEST_CASE("base64 decodes including padding") {
    auto d = base64_decode("aGVsbG8=");           // "hello"
    REQUIRE(d.has_value());
    std::string s(d->begin(), d->end());
    CHECK(s == "hello");
    CHECK(base64_decode("")->empty());
    CHECK_FALSE(base64_decode("!!!!").has_value()); // invalid alphabet
}

TEST_CASE("parse_slot_meta: 200 with a slot") {
    const char* body = R"({"slot":{"titleId":"0100000000010000","label":"Zelda","revision":4,"updatedAt":1733242800}})";
    auto m = parse_slot_meta(body, 200);
    REQUIRE(m.has_value());
    CHECK(m->exists);
    CHECK(m->revision == 4);
    CHECK(m->label == "Zelda");
    CHECK(m->updatedAt == 1733242800);
}

TEST_CASE("parse_slot_meta: 404 means no slot, not an error") {
    auto m = parse_slot_meta(R"({"error":"save_not_found"})", 404);
    REQUIRE(m.has_value());
    CHECK_FALSE(m->exists);
}

TEST_CASE("parse_slot_meta: other status is an error (nullopt)") {
    CHECK_FALSE(parse_slot_meta(R"({"error":"boom"})", 500).has_value());
}

TEST_CASE("parse_slot_data: meta plus decoded blob") {
    const char* body = R"({"slot":{"titleId":"0100000000010000","label":"x","revision":2,"updatedAt":10,"data":"aGk="}})";
    auto d = parse_slot_data(body, 200);
    REQUIRE(d.has_value());
    CHECK(d->meta.exists);
    CHECK(d->meta.revision == 2);
    std::string blob(d->data.begin(), d->data.end());
    CHECK(blob == "hi");
}

TEST_CASE("parse_push_revision reads the new revision") {
    auto r = parse_push_revision(R"({"ok":true,"slot":{"revision":7}})");
    REQUIRE(r.has_value());
    CHECK(*r == 7);
    CHECK_FALSE(parse_push_revision(R"({"nope":1})").has_value());
}

TEST_CASE("parse_error_message prefers the error field, falls back to status") {
    CHECK(parse_error_message(R"({"error":"revision_conflict"})", 409) == "revision_conflict");
    CHECK(parse_error_message("not json", 500) == "http_500");
}
```

- [ ] **Step 2: Run it to confirm it fails**

Run: `cd tests && make test`
Expected: compile error — `core/saves/cloud_save_json.hpp` not found.

- [ ] **Step 3: Write the header**

`source/core/saves/cloud_save_json.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace thomaz::core {

struct CloudSlotMeta {
    bool         exists    = false;
    int          revision  = 0;
    std::string  label;
    std::int64_t updatedAt = 0;
};

struct CloudSlotData {
    CloudSlotMeta             meta;
    std::vector<std::uint8_t> data;
};

// 200 -> meta with exists=true; 404 -> meta with exists=false; else nullopt.
std::optional<CloudSlotMeta> parse_slot_meta(const std::string& body, long status);

// 200 -> meta + decoded blob; 404 -> meta exists=false, empty data; else nullopt.
std::optional<CloudSlotData> parse_slot_data(const std::string& body, long status);

// New revision from a successful PUT body, or nullopt.
std::optional<int> parse_push_revision(const std::string& body);

// "error" string from the body, or "http_<status>" if absent/unparseable.
std::string parse_error_message(const std::string& body, long status);

// Standard base64 decode; nullopt on invalid input.
std::optional<std::vector<std::uint8_t>> base64_decode(const std::string& in);

} // namespace thomaz::core
```

- [ ] **Step 4: Write the implementation**

`source/core/saves/cloud_save_json.cpp`:

```cpp
#include "core/saves/cloud_save_json.hpp"
#include <nlohmann/json.hpp>

namespace thomaz::core {

using nlohmann::json;

namespace {
json safe_parse(const std::string& body) {
    return json::parse(body, nullptr, /*allow_exceptions=*/false);
}

CloudSlotMeta read_meta(const json& slot) {
    CloudSlotMeta m;
    m.exists = true;
    if (slot.contains("revision") && slot["revision"].is_number_integer())
        m.revision = slot["revision"].get<int>();
    if (slot.contains("label") && slot["label"].is_string())
        m.label = slot["label"].get<std::string>();
    if (slot.contains("updatedAt") && slot["updatedAt"].is_number_integer())
        m.updatedAt = slot["updatedAt"].get<std::int64_t>();
    return m;
}
} // namespace

std::optional<CloudSlotMeta> parse_slot_meta(const std::string& body, long status) {
    if (status == 404) return CloudSlotMeta{}; // exists=false
    if (status < 200 || status >= 300) return std::nullopt;
    json j = safe_parse(body);
    if (!j.is_object() || !j.contains("slot") || !j["slot"].is_object())
        return std::nullopt;
    return read_meta(j["slot"]);
}

std::optional<CloudSlotData> parse_slot_data(const std::string& body, long status) {
    if (status == 404) return CloudSlotData{}; // meta.exists=false, empty data
    if (status < 200 || status >= 300) return std::nullopt;
    json j = safe_parse(body);
    if (!j.is_object() || !j.contains("slot") || !j["slot"].is_object())
        return std::nullopt;
    CloudSlotData d;
    d.meta = read_meta(j["slot"]);
    const json& slot = j["slot"];
    if (slot.contains("data") && slot["data"].is_string()) {
        auto decoded = base64_decode(slot["data"].get<std::string>());
        if (!decoded) return std::nullopt;
        d.data = std::move(*decoded);
    }
    return d;
}

std::optional<int> parse_push_revision(const std::string& body) {
    json j = safe_parse(body);
    if (j.is_object() && j.contains("slot") && j["slot"].is_object()) {
        const json& slot = j["slot"];
        if (slot.contains("revision") && slot["revision"].is_number_integer())
            return slot["revision"].get<int>();
    }
    return std::nullopt;
}

std::string parse_error_message(const std::string& body, long status) {
    json j = safe_parse(body);
    if (j.is_object() && j.contains("error") && j["error"].is_string())
        return j["error"].get<std::string>();
    return "http_" + std::to_string(status);
}

std::optional<std::vector<std::uint8_t>> base64_decode(const std::string& in) {
    static const std::string kAlpha =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::vector<std::uint8_t> out;
    int buf = 0, bits = 0;
    for (char c : in) {
        if (c == '=' ) break;
        if (c == '\n' || c == '\r') continue;
        int v = val(c);
        if (v < 0) return std::nullopt;
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((std::uint8_t)((buf >> bits) & 0xFF));
        }
    }
    return out;
}

} // namespace thomaz::core
```

- [ ] **Step 5: Run the test**

Run: `cd tests && make test`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add source/core/saves/cloud_save_json.hpp source/core/saves/cloud_save_json.cpp tests/test_cloud_save_json.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(saves): cloud_save_json parsers + base64 decode

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5: ICloudSaveClient interface + FakeCloudSaveClient

**Files:**
- Create: `source/platform/saves/cloud_save_client.hpp`
- Create: `source/platform/saves/fake_cloud_save_client.hpp`, `source/platform/saves/fake_cloud_save_client.cpp`
- Test: `tests/test_fake_cloud_save_client.cpp`
- Modify: `tests/Makefile`

- [ ] **Step 1: Add the fake client to the test Makefile**

In `tests/Makefile`, append `../source/platform/saves/fake_cloud_save_client.cpp` to `SRCS`:

```make
SRCS     := $(wildcard *.cpp) $(wildcard ../source/core/*.cpp) $(wildcard ../source/core/feed/*.cpp) $(wildcard ../source/core/saves/*.cpp) ../source/platform/cheat_store.cpp ../source/platform/feed/http_feed_client.cpp ../source/platform/app_settings.cpp ../source/platform/saves/fake_cloud_save_client.cpp
```

- [ ] **Step 2: Write the interface header**

`source/platform/saves/cloud_save_client.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace thomaz {

// Error sentinel set when the API returns 401. The UI maps this to a re-login
// prompt (we do not auto-refresh the token from the save client).
inline constexpr const char* kCloudAuthExpired = "unauthorized";

struct CloudStatus {
    bool         ok        = false; // request completed (exists may still be false)
    bool         exists    = false;
    int          revision  = 0;
    std::string  label;
    std::int64_t updatedAt = 0;
    std::string  error;             // set when !ok
};

struct CloudPull {
    bool                      ok        = false;
    bool                      exists    = false;
    int                       revision  = 0;
    std::string               label;
    std::int64_t              updatedAt = 0;
    std::vector<std::uint8_t> blob;
    std::string               error;
};

struct CloudPush {
    bool        ok          = false;
    bool        conflict    = false; // HTTP 409 (revision_conflict)
    int         newRevision = 0;
    std::string error;
};

// Talks to the thomaz-api /saves endpoints. All methods run on a brls::async
// worker thread and must not touch the UI. The token is supplied per call by
// the UI (read from auth_store).
class ICloudSaveClient {
  public:
    virtual ~ICloudSaveClient() = default;

    virtual CloudStatus getStatus(const std::string& token, std::uint64_t titleId) = 0;
    virtual CloudPull   pull(const std::string& token, std::uint64_t titleId) = 0;
    virtual CloudPush   push(const std::string& token, std::uint64_t titleId,
                             const std::vector<std::uint8_t>& blob,
                             const std::string& label, int revision) = 0;
};

} // namespace thomaz
```

- [ ] **Step 3: Write the failing test**

`tests/test_fake_cloud_save_client.cpp`:

```cpp
#include "doctest.h"
#include "platform/saves/fake_cloud_save_client.hpp"

using namespace thomaz;

static std::vector<std::uint8_t> bytes(const std::string& s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

TEST_CASE("status is empty before any push") {
    FakeCloudSaveClient c;
    auto s = c.getStatus("tok", 0x100ULL);
    CHECK(s.ok);
    CHECK_FALSE(s.exists);
}

TEST_CASE("push then status/pull reflect the upload") {
    FakeCloudSaveClient c;
    auto p = c.push("tok", 0x100ULL, bytes("save-bytes"), "Zelda", 0);
    REQUIRE(p.ok);
    CHECK(p.newRevision == 1);

    auto s = c.getStatus("tok", 0x100ULL);
    CHECK(s.exists);
    CHECK(s.revision == 1);
    CHECK(s.label == "Zelda");

    auto d = c.pull("tok", 0x100ULL);
    REQUIRE(d.ok);
    CHECK(d.exists);
    CHECK(d.revision == 1);
    std::string blob(d.blob.begin(), d.blob.end());
    CHECK(blob == "save-bytes");
}

TEST_CASE("pushing with a stale revision conflicts") {
    FakeCloudSaveClient c;
    c.push("tok", 0x100ULL, bytes("v1"), "g", 0);   // -> revision 1
    auto stale = c.push("tok", 0x100ULL, bytes("v2"), "g", 0); // expected 1, sent 0
    CHECK_FALSE(stale.ok);
    CHECK(stale.conflict);

    auto good = c.push("tok", 0x100ULL, bytes("v2"), "g", 1); // correct revision
    CHECK(good.ok);
    CHECK(good.newRevision == 2);
}
```

- [ ] **Step 4: Run it to confirm it fails**

Run: `cd tests && make test`
Expected: compile error — `platform/saves/fake_cloud_save_client.hpp` not found.

- [ ] **Step 5: Write the fake header + implementation**

`source/platform/saves/fake_cloud_save_client.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "platform/saves/cloud_save_client.hpp"

namespace thomaz {

// In-memory ICloudSaveClient for tests and offline desktop use. Mirrors the
// API's optimistic-concurrency rule: a PUT must send the current revision.
class FakeCloudSaveClient : public ICloudSaveClient {
  public:
    CloudStatus getStatus(const std::string& token, std::uint64_t titleId) override;
    CloudPull   pull(const std::string& token, std::uint64_t titleId) override;
    CloudPush   push(const std::string& token, std::uint64_t titleId,
                     const std::vector<std::uint8_t>& blob,
                     const std::string& label, int revision) override;

  private:
    struct Slot {
        int                       revision = 0;
        std::string               label;
        std::vector<std::uint8_t> blob;
    };
    std::map<std::uint64_t, Slot> slots;
};

} // namespace thomaz
```

`source/platform/saves/fake_cloud_save_client.cpp`:

```cpp
#include "platform/saves/fake_cloud_save_client.hpp"

namespace thomaz {

CloudStatus FakeCloudSaveClient::getStatus(const std::string&, std::uint64_t titleId) {
    CloudStatus s;
    s.ok = true;
    auto it = slots.find(titleId);
    if (it != slots.end()) {
        s.exists   = true;
        s.revision = it->second.revision;
        s.label    = it->second.label;
    }
    return s;
}

CloudPull FakeCloudSaveClient::pull(const std::string&, std::uint64_t titleId) {
    CloudPull p;
    p.ok = true;
    auto it = slots.find(titleId);
    if (it != slots.end()) {
        p.exists   = true;
        p.revision = it->second.revision;
        p.label    = it->second.label;
        p.blob     = it->second.blob;
    }
    return p;
}

CloudPush FakeCloudSaveClient::push(const std::string&, std::uint64_t titleId,
                                    const std::vector<std::uint8_t>& blob,
                                    const std::string& label, int revision) {
    CloudPush r;
    auto it = slots.find(titleId);
    int current = it == slots.end() ? 0 : it->second.revision;
    if (revision != current) {
        r.conflict = true;
        return r;
    }
    Slot& slot   = slots[titleId];
    slot.revision = current + 1;
    slot.label    = label;
    slot.blob     = blob;
    r.ok          = true;
    r.newRevision = slot.revision;
    return r;
}

} // namespace thomaz
```

- [ ] **Step 6: Run the test**

Run: `cd tests && make test`
Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add source/platform/saves/cloud_save_client.hpp source/platform/saves/fake_cloud_save_client.hpp source/platform/saves/fake_cloud_save_client.cpp tests/test_fake_cloud_save_client.cpp tests/Makefile
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(saves): ICloudSaveClient + in-memory FakeCloudSaveClient

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 6: HttpCloudSaveClient (real API)

**Files:**
- Create: `source/platform/saves/http_cloud_save_client.hpp`, `source/platform/saves/http_cloud_save_client.cpp`
- Test: `tests/test_http_cloud_save_client.cpp`
- Modify: `tests/Makefile`

- [ ] **Step 1: Add the http client to the test Makefile**

In `tests/Makefile`, append `../source/platform/saves/http_cloud_save_client.cpp` to `SRCS`.

- [ ] **Step 2: Write the failing test (with a recording fake IHttpClient)**

`tests/test_http_cloud_save_client.cpp`:

```cpp
#include "doctest.h"
#include "platform/saves/http_cloud_save_client.hpp"

using namespace thomaz;

namespace {
// Records the last request and returns a canned response.
struct StubHttp : IHttpClient {
    HttpRequest  last;
    HttpResponse next;
    HttpResponse request(const HttpRequest& req) override {
        last = req;
        return next;
    }
};
} // namespace

TEST_CASE("getStatus issues a GET with bearer and parses the slot") {
    StubHttp http;
    http.next = HttpResponse{ 200,
        R"({"slot":{"titleId":"0100000000010000","label":"Z","revision":4,"updatedAt":9}})" };
    HttpCloudSaveClient c(&http, "http://api.test");

    auto s = c.getStatus("mytoken", 0x0100000000010000ULL);
    CHECK(s.ok);
    CHECK(s.exists);
    CHECK(s.revision == 4);
    CHECK(http.last.method == HttpMethod::Get);
    CHECK(http.last.url == "http://api.test/saves/0100000000010000");
    bool hasBearer = false;
    for (auto& h : http.last.headers)
        if (h.first == "Authorization" && h.second == "Bearer mytoken") hasBearer = true;
    CHECK(hasBearer);
}

TEST_CASE("getStatus maps 404 to exists=false (still ok)") {
    StubHttp http;
    http.next = HttpResponse{ 404, R"({"error":"save_not_found"})" };
    HttpCloudSaveClient c(&http, "http://api.test");
    auto s = c.getStatus("t", 0x1ULL);
    CHECK(s.ok);
    CHECK_FALSE(s.exists);
}

TEST_CASE("getStatus maps 401 to an auth-expired error") {
    StubHttp http;
    http.next = HttpResponse{ 401, R"({"error":"unauthorized"})" };
    HttpCloudSaveClient c(&http, "http://api.test");
    auto s = c.getStatus("t", 0x1ULL);
    CHECK_FALSE(s.ok);
    CHECK(s.error == kCloudAuthExpired);
}

TEST_CASE("pull GETs with includeData and decodes the blob") {
    StubHttp http;
    http.next = HttpResponse{ 200,
        R"({"slot":{"titleId":"01","label":"x","revision":2,"updatedAt":1,"data":"aGk="}})" };
    HttpCloudSaveClient c(&http, "http://api.test");
    auto d = c.pull("t", 0x1ULL);
    REQUIRE(d.ok);
    CHECK(d.exists);
    CHECK(d.revision == 2);
    std::string blob(d.blob.begin(), d.blob.end());
    CHECK(blob == "hi");
    CHECK(http.last.url == "http://api.test/saves/0000000000000001?includeData=1");
}

TEST_CASE("push sends multipart and reads the new revision") {
    StubHttp http;
    http.next = HttpResponse{ 200, R"({"ok":true,"slot":{"revision":3}})" };
    HttpCloudSaveClient c(&http, "http://api.test");
    std::vector<std::uint8_t> blob = { 1, 2, 3 };
    auto r = c.push("t", 0x1ULL, blob, "Zelda", 2);
    REQUIRE(r.ok);
    CHECK(r.newRevision == 3);
    CHECK(http.last.method == HttpMethod::Put);
    REQUIRE(http.last.files.size() == 1);
    CHECK(http.last.files[0].field == "data");
    CHECK(http.last.files[0].bytes == blob);
    bool sentRevision = false, sentLabel = false;
    for (auto& f : http.last.fields) {
        if (f.first == "revision" && f.second == "2") sentRevision = true;
        if (f.first == "label" && f.second == "Zelda") sentLabel = true;
    }
    CHECK(sentRevision);
    CHECK(sentLabel);
}

TEST_CASE("push maps 409 to a conflict and 413 to a too-large error") {
    HttpCloudSaveClient c0(nullptr, "http://api.test"); // not used; placeholder
    StubHttp http;
    http.next = HttpResponse{ 409, R"({"error":"revision_conflict"})" };
    HttpCloudSaveClient c(&http, "http://api.test");
    auto conflict = c.push("t", 0x1ULL, { 1 }, "g", 0);
    CHECK_FALSE(conflict.ok);
    CHECK(conflict.conflict);

    http.next = HttpResponse{ 413, R"({"error":"save_too_large"})" };
    auto tooBig = c.push("t", 0x1ULL, { 1 }, "g", 0);
    CHECK_FALSE(tooBig.ok);
    CHECK_FALSE(tooBig.conflict);
    CHECK(tooBig.error == "save_too_large");
}
```

- [ ] **Step 3: Run it to confirm it fails**

Run: `cd tests && make test`
Expected: compile error — `platform/saves/http_cloud_save_client.hpp` not found.

- [ ] **Step 4: Write the header**

`source/platform/saves/http_cloud_save_client.hpp`:

```cpp
#pragma once
#include <string>
#include "platform/saves/cloud_save_client.hpp"
#include "platform/http_client.hpp"

namespace thomaz {

// Real ICloudSaveClient backed by the thomaz-api over HTTP. Stateless beyond
// the base URL; the access token is passed per call (read from auth_store by
// the UI). On 401 the result error is kCloudAuthExpired (no auto-refresh).
class HttpCloudSaveClient : public ICloudSaveClient {
  public:
    HttpCloudSaveClient(IHttpClient* http, std::string baseUrl);

    CloudStatus getStatus(const std::string& token, std::uint64_t titleId) override;
    CloudPull   pull(const std::string& token, std::uint64_t titleId) override;
    CloudPush   push(const std::string& token, std::uint64_t titleId,
                     const std::vector<std::uint8_t>& blob,
                     const std::string& label, int revision) override;

  private:
    std::string savesUrl(std::uint64_t titleId) const;

    IHttpClient* http;
    std::string  baseUrl; // no trailing slash
};

} // namespace thomaz
```

- [ ] **Step 5: Write the implementation**

`source/platform/saves/http_cloud_save_client.cpp`:

```cpp
#include "platform/saves/http_cloud_save_client.hpp"
#include "core/saves/cloud_save_json.hpp"
#include <cstdio>
#include <utility>

namespace thomaz {

namespace {
std::string titleIdHex(std::uint64_t id) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)id);
    return std::string(buf);
}
} // namespace

HttpCloudSaveClient::HttpCloudSaveClient(IHttpClient* http, std::string baseUrl)
    : http(http), baseUrl(std::move(baseUrl)) {}

std::string HttpCloudSaveClient::savesUrl(std::uint64_t titleId) const {
    return baseUrl + "/saves/" + titleIdHex(titleId);
}

CloudStatus HttpCloudSaveClient::getStatus(const std::string& token, std::uint64_t titleId) {
    HttpRequest req;
    req.method = HttpMethod::Get;
    req.url    = savesUrl(titleId);
    req.headers.push_back({ "Authorization", "Bearer " + token });

    HttpResponse resp = http->request(req);
    CloudStatus s;
    if (resp.status == 401) { s.error = kCloudAuthExpired; return s; }

    auto meta = core::parse_slot_meta(resp.body, resp.status);
    if (!meta) { s.error = core::parse_error_message(resp.body, resp.status); return s; }

    s.ok        = true;
    s.exists    = meta->exists;
    s.revision  = meta->revision;
    s.label     = meta->label;
    s.updatedAt = meta->updatedAt;
    return s;
}

CloudPull HttpCloudSaveClient::pull(const std::string& token, std::uint64_t titleId) {
    HttpRequest req;
    req.method = HttpMethod::Get;
    req.url    = savesUrl(titleId) + "?includeData=1";
    req.headers.push_back({ "Authorization", "Bearer " + token });

    HttpResponse resp = http->request(req);
    CloudPull p;
    if (resp.status == 401) { p.error = kCloudAuthExpired; return p; }

    auto data = core::parse_slot_data(resp.body, resp.status);
    if (!data) { p.error = core::parse_error_message(resp.body, resp.status); return p; }

    p.ok        = true;
    p.exists    = data->meta.exists;
    p.revision  = data->meta.revision;
    p.label     = data->meta.label;
    p.updatedAt = data->meta.updatedAt;
    p.blob      = std::move(data->data);
    return p;
}

CloudPush HttpCloudSaveClient::push(const std::string& token, std::uint64_t titleId,
                                    const std::vector<std::uint8_t>& blob,
                                    const std::string& label, int revision) {
    HttpRequest req;
    req.method = HttpMethod::Put;
    req.url    = savesUrl(titleId);
    req.headers.push_back({ "Authorization", "Bearer " + token });
    req.fields.push_back({ "label", label });
    req.fields.push_back({ "revision", std::to_string(revision) });
    req.files.push_back(MultipartFile{ "data", titleIdHex(titleId) + ".bin",
                                       "application/octet-stream", blob });

    HttpResponse resp = http->request(req);
    CloudPush r;
    if (resp.status == 401) { r.error = kCloudAuthExpired; return r; }
    if (resp.status == 409) { r.conflict = true; return r; }
    if (!resp.ok()) { r.error = core::parse_error_message(resp.body, resp.status); return r; }

    auto rev = core::parse_push_revision(resp.body);
    if (!rev) { r.error = core::parse_error_message(resp.body, resp.status); return r; }
    r.ok          = true;
    r.newRevision = *rev;
    return r;
}

} // namespace thomaz
```

- [ ] **Step 6: Run the test**

Run: `cd tests && make test`
Expected: PASS. (Note: the `c0(nullptr, ...)` placeholder in the 409 test is never called; it just exercises construction.)

- [ ] **Step 7: Commit**

```bash
git add source/platform/saves/http_cloud_save_client.hpp source/platform/saves/http_cloud_save_client.cpp tests/test_http_cloud_save_client.cpp tests/Makefile
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(saves): HttpCloudSaveClient over the real API

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 7: save_backup_io + ISaveService methods (fake)

**Files:**
- Create: `source/platform/saves/save_backup_io.hpp`, `source/platform/saves/save_backup_io.cpp`
- Modify: `source/platform/save_service.hpp`
- Modify: `source/platform/save_service_fake.hpp`, `source/platform/save_service_fake.cpp`

This task is exercised by the desktop build + smoke run (filesystem side effects, not unit-tested). The Switch impl follows in Task 8.

- [ ] **Step 1: Write the shared backup-writer header**

`source/platform/saves/save_backup_io.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <string>
#include "core/saves/save_package.hpp"

namespace thomaz {

// Writes a package as a new timestamped local backup under saves_root(), with a
// manifest.json listing the profiles (first path segment of each file). Returns
// true; on failure returns false and sets *outError. Shared by both the fake
// and Switch save services so the import path is identical.
bool write_package_as_backup(std::uint64_t title_id, const std::string& game_name,
                             const core::SavePackage& pkg, std::string* outError);

} // namespace thomaz
```

- [ ] **Step 2: Write the shared backup-writer implementation**

`source/platform/saves/save_backup_io.cpp`:

```cpp
#include "platform/saves/save_backup_io.hpp"

#include <ctime>
#include <set>
#include <vector>

#include "core/backup_store.hpp"
#include "platform/cheat_store.hpp" // write_text_file

namespace thomaz {

namespace {
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

// First path segment ("aaaa/save.dat" -> "aaaa"); empty if none.
std::string first_segment(const std::string& path) {
    auto slash = path.find('/');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}
} // namespace

bool write_package_as_backup(std::uint64_t title_id, const std::string& game_name,
                             const core::SavePackage& pkg, std::string* outError) {
    std::string ts  = now_timestamp();
    std::string dir = core::backup_dir(core::saves_root(), title_id, ts);

    std::set<std::string> profileSet;
    for (const auto& f : pkg.files) {
        std::string body(f.bytes.begin(), f.bytes.end());
        if (!write_text_file(dir + "/" + f.path, body)) {
            if (outError) *outError = "could not write save file";
            return false;
        }
        std::string seg = first_segment(f.path);
        if (!seg.empty()) profileSet.insert(seg);
    }

    core::ManifestInfo m;
    m.game_name = game_name;
    m.title_id  = title_id;
    m.timestamp = ts;
    m.profiles.assign(profileSet.begin(), profileSet.end());
    if (!write_text_file(dir + "/manifest.json", core::build_manifest(m))) {
        if (outError) *outError = "could not write manifest";
        return false;
    }
    return true;
}

} // namespace thomaz
```

- [ ] **Step 3: Add the two methods to the ISaveService interface**

In `source/platform/save_service.hpp`, add inside the class (after `restore`), and add `#include <cstdint>` and `#include <vector>` if not already present (they are via existing includes):

```cpp
    // Read the title's active save (all profiles) into an opaque blob suitable
    // for upload. Returns empty + sets *outError on failure.
    virtual std::vector<std::uint8_t> packageActiveSave(std::uint64_t title_id,
                                                        std::string* outError) = 0;

    // Write a downloaded blob as a new local backup (so it appears in the
    // history and can be restored). Returns false + sets *outError on failure.
    virtual bool importPackageAsBackup(std::uint64_t title_id,
                                       const std::vector<std::uint8_t>& blob,
                                       std::string* outError) = 0;
```

- [ ] **Step 4: Declare them in the fake header**

In `source/platform/save_service_fake.hpp`, add to the class:

```cpp
    std::vector<std::uint8_t> packageActiveSave(std::uint64_t title_id,
                                                std::string* outError) override;
    bool importPackageAsBackup(std::uint64_t title_id,
                               const std::vector<std::uint8_t>& blob,
                               std::string* outError) override;
```

- [ ] **Step 5: Implement them in the fake**

In `source/platform/save_service_fake.cpp`, add these includes near the top (after the existing includes):

```cpp
#include "core/saves/save_package.hpp"
#include "platform/saves/save_backup_io.hpp"
```

And add the two methods before the closing `} // namespace thomaz`:

```cpp
std::vector<std::uint8_t> FakeSaveService::packageActiveSave(std::uint64_t,
                                                             std::string* outError) {
    // Desktop stand-in: one dummy profile with one dummy file, matching what
    // backup() writes, so the cloud round-trip is exercisable without a console.
    core::SavePackage pkg;
    std::string dummy = "fake save bytes";
    pkg.files.push_back({ "11111111111111111111111111111111/save.dat",
                          std::vector<std::uint8_t>(dummy.begin(), dummy.end()) });
    (void)outError;
    return pack_save_package(pkg);
}

bool FakeSaveService::importPackageAsBackup(std::uint64_t title_id,
                                            const std::vector<std::uint8_t>& blob,
                                            std::string* outError) {
    auto pkg = core::unpack_save_package(blob);
    if (!pkg) {
        if (outError) *outError = "corrupted cloud save";
        return false;
    }
    return write_package_as_backup(title_id, "", *pkg, outError);
}
```

- [ ] **Step 6: Build the desktop app**

Run: `./scripts/build-desktop.sh`
Expected: `BUILD EXIT: 0` (the new files are globbed by CMake; `save_service_fake` now satisfies the interface).

- [ ] **Step 7: Commit**

```bash
git add source/platform/saves/save_backup_io.hpp source/platform/saves/save_backup_io.cpp source/platform/save_service.hpp source/platform/save_service_fake.hpp source/platform/save_service_fake.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(saves): ISaveService package/import + shared backup writer (fake)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 8: ISaveService methods (Switch / libnx)

**Files:**
- Modify: `source/platform/save_service_switch.hpp`, `source/platform/save_service_switch.cpp`

> This code only compiles under the Switch (devkitPro) toolchain — it is guarded by `#ifdef __SWITCH__` and is **not** built by `./scripts/build-desktop.sh`. Verify by code review and (if available) a Switch build; the desktop build must remain green because this file is excluded there.

- [ ] **Step 1: Declare the methods in the Switch header**

In `source/platform/save_service_switch.hpp`, add to the class:

```cpp
    std::vector<std::uint8_t> packageActiveSave(std::uint64_t title_id,
                                                std::string* outError) override;
    bool importPackageAsBackup(std::uint64_t title_id,
                               const std::vector<std::uint8_t>& blob,
                               std::string* outError) override;
```

- [ ] **Step 2: Add includes + a recursive in-memory reader**

In `source/platform/save_service_switch.cpp`, add after the existing includes:

```cpp
#include "core/saves/save_package.hpp"
#include "platform/saves/save_backup_io.hpp"
```

Inside the anonymous `namespace { ... }` (after `copy_tree`), add a reader that walks a mounted save into package entries:

```cpp
// Recursively read every file under `src` into the package, prefixing each
// path with `prefix` (the profile's uid_hex). Returns false on any read error.
bool read_tree(const std::string& src, const std::string& prefix,
               core::SavePackage& pkg) {
    DIR* d = ::opendir(src.c_str());
    if (!d) return false;
    bool ok = true;
    struct dirent* e;
    while (ok && (e = ::readdir(d)) != nullptr) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        std::string s = src + "/" + name;
        std::string rel = prefix.empty() ? name : prefix + "/" + name;
        struct stat st;
        if (::stat(s.c_str(), &st) != 0) { ok = false; break; }
        if (S_ISDIR(st.st_mode)) {
            ok = read_tree(s, rel, pkg);
        } else {
            FILE* in = std::fopen(s.c_str(), "rb");
            if (!in) { ok = false; break; }
            std::vector<std::uint8_t> bytes;
            char buf[8192];
            size_t n;
            while ((n = std::fread(buf, 1, sizeof(buf), in)) > 0)
                bytes.insert(bytes.end(), buf, buf + n);
            std::fclose(in);
            pkg.files.push_back({ rel, std::move(bytes) });
        }
    }
    ::closedir(d);
    return ok;
}
```

- [ ] **Step 3: Implement packageActiveSave**

Add before the closing `} // namespace thomaz` (inside `#ifdef __SWITCH__`):

```cpp
std::vector<std::uint8_t> NsSaveService::packageActiveSave(std::uint64_t title_id,
                                                           std::string* outError) {
    accountInitialize(AccountServiceType_System);
    core::SavePackage pkg;
    bool any = false;
    for (auto& p : all_profiles()) {
        AccountUid uid;
        std::sscanf(p.uid_hex.c_str(), "%016lx%016lx",
                    (unsigned long*)&uid.uid[0], (unsigned long*)&uid.uid[1]);
        if (R_FAILED(fsdevMountSaveData(kMount, title_id, uid)))
            continue; // no save for this profile
        std::string mountRoot = std::string(kMount) + ":/";
        if (read_tree(mountRoot, p.uid_hex, pkg))
            any = true;
        fsdevUnmountDevice(kMount);
    }
    accountExit();
    if (!any) {
        if (outError) *outError = "no save data";
        return {};
    }
    return core::pack_save_package(pkg);
}

bool NsSaveService::importPackageAsBackup(std::uint64_t title_id,
                                          const std::vector<std::uint8_t>& blob,
                                          std::string* outError) {
    auto pkg = core::unpack_save_package(blob);
    if (!pkg) {
        if (outError) *outError = "corrupted cloud save";
        return false;
    }
    return write_package_as_backup(title_id, "", *pkg, outError);
}
```

- [ ] **Step 4: Verify the desktop build is unaffected**

Run: `./scripts/build-desktop.sh`
Expected: `BUILD EXIT: 0` (this file is excluded on desktop; the build must stay green).

- [ ] **Step 5: Commit**

```bash
git add source/platform/save_service_switch.hpp source/platform/save_service_switch.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(saves): NsSaveService package/import via libnx save mount

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 9: sync_store — persist the synced revision

**Files:**
- Create: `source/platform/saves/sync_store.hpp`, `source/platform/saves/sync_store.cpp`

File I/O wrapper around the Task 2 codec, mirroring `auth_store`/`app_settings`. Exercised by the build and the UI flow.

- [ ] **Step 1: Write the header**

`source/platform/saves/sync_store.hpp`:

```cpp
#pragma once
#include <cstdint>

namespace thomaz {

// Last cloud revision synced for a title, persisted on the SD (Switch) or the
// working dir (desktop). 0 if never synced. Backed by the save_sync_state codec.
int  load_synced_revision(std::uint64_t title_id);
void save_synced_revision(std::uint64_t title_id, int revision);

} // namespace thomaz
```

- [ ] **Step 2: Write the implementation**

`source/platform/saves/sync_store.cpp`:

```cpp
#include "platform/saves/sync_store.hpp"
#include "core/saves/save_sync_state.hpp"
#include "platform/cheat_store.hpp" // read_text_file / write_text_file
#include <map>
#include <string>

namespace thomaz {

namespace {
std::string sync_file() {
#ifdef __SWITCH__
    return "/switch/thomaz/config/save_sync.txt";
#else
    return "thomaz-cache/save_sync.txt";
#endif
}
} // namespace

int load_synced_revision(std::uint64_t title_id) {
    auto body = read_text_file(sync_file());
    if (!body) return 0;
    auto state = core::parse_sync_state(*body);
    return core::synced_revision(state, title_id);
}

void save_synced_revision(std::uint64_t title_id, int revision) {
    std::map<std::uint64_t, int> state;
    if (auto body = read_text_file(sync_file()))
        state = core::parse_sync_state(*body);
    state[title_id] = revision;
    write_text_file(sync_file(), core::serialize_sync_state(state));
}

} // namespace thomaz
```

- [ ] **Step 3: Build the desktop app**

Run: `./scripts/build-desktop.sh`
Expected: `BUILD EXIT: 0`.

- [ ] **Step 4: Commit**

```bash
git add source/platform/saves/sync_store.hpp source/platform/saves/sync_store.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(saves): sync_store persists the synced revision per title

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 10: Wire ICloudSaveClient + IFeedClient through to SaveDetailActivity

**Files:**
- Modify: `source/app/home_activity.hpp`, `source/app/home_activity.cpp`
- Modify: `source/app/save_manager_activity.hpp`, `source/app/save_manager_activity.cpp`
- Modify: `source/app/save_detail_activity.hpp`, `source/app/save_detail_activity.cpp`
- Modify: `source/main.cpp`

Plumbing only — no behavior yet. The detail activity stores the pointers; the Cloud section comes in Tasks 11–13.

- [ ] **Step 1: Extend HomeActivity to carry the cloud client**

In `source/app/home_activity.hpp`, add the include and constructor param + member:

```cpp
#include "platform/saves/cloud_save_client.hpp"
```

Change the constructor signature and add the member:

```cpp
    HomeActivity(ITitleService* titleService, IHttpClient* http, ISaveService* saveService,
                 IFeedClient* feed, IAlbumSource* album, ICloudSaveClient* cloudSaves);
```
```cpp
    ICloudSaveClient* cloudSaves;
```

In `source/app/home_activity.cpp`, update the constructor definition and initializer list:

```cpp
HomeActivity::HomeActivity(ITitleService* titleService, IHttpClient* http, ISaveService* saveService,
                           IFeedClient* feed, IAlbumSource* album, ICloudSaveClient* cloudSaves)
    : titleService(titleService), http(http), saveService(saveService), feed(feed),
      album(album), cloudSaves(cloudSaves)
{
}
```

And where it constructs `SaveManagerActivity` (around line 59), pass the new pointers:

```cpp
                new SaveManagerActivity(this->titleService, this->saveService,
                                        this->cloudSaves, this->feed));
```

- [ ] **Step 2: Extend SaveManagerActivity**

In `source/app/save_manager_activity.hpp`, add includes and constructor params + members:

```cpp
#include "platform/saves/cloud_save_client.hpp"
#include "platform/feed/feed_client.hpp"
```
```cpp
    SaveManagerActivity(ITitleService* titleService, ISaveService* saveService,
                        ICloudSaveClient* cloudSaves, IFeedClient* feed);
```
```cpp
    ICloudSaveClient* cloudSaves;
    IFeedClient* feed;
```

In `source/app/save_manager_activity.cpp`, update the constructor:

```cpp
SaveManagerActivity::SaveManagerActivity(ITitleService* titleService, ISaveService* saveService,
                                         ICloudSaveClient* cloudSaves, IFeedClient* feed)
    : titleService(titleService), saveService(saveService), cloudSaves(cloudSaves), feed(feed)
{
}
```

In `populate()`, capture the new pointers and pass them when constructing `SaveDetailActivity`. Change the capture of the row click lambda from `[rowTitle, save]` to also capture the members:

```cpp
        ICloudSaveClient* cloud = this->cloudSaves;
        IFeedClient* feedClient = this->feed;
        InstalledTitle rowTitle = title;
        row->registerClickAction([rowTitle, save, cloud, feedClient](brls::View*) {
            brls::Application::pushActivity(
                new SaveDetailActivity(rowTitle, save, cloud, feedClient));
            return true;
        });
```

(Define `ICloudSaveClient* cloud` / `IFeedClient* feedClient` once before the loop, alongside the existing `ISaveService* save = this->saveService;`.)

- [ ] **Step 3: Extend SaveDetailActivity (storage only)**

In `source/app/save_detail_activity.hpp`, add includes, constructor params, and members:

```cpp
#include "platform/saves/cloud_save_client.hpp"
#include "platform/feed/feed_client.hpp"
```
```cpp
    SaveDetailActivity(InstalledTitle title, ISaveService* saveService,
                       ICloudSaveClient* cloudSaves, IFeedClient* feed);
```
```cpp
    ICloudSaveClient* cloudSaves;
    IFeedClient* feed;
```

In `source/app/save_detail_activity.cpp`, update the constructor:

```cpp
SaveDetailActivity::SaveDetailActivity(InstalledTitle title, ISaveService* saveService,
                                       ICloudSaveClient* cloudSaves, IFeedClient* feed)
    : title(std::move(title)), saveService(saveService), cloudSaves(cloudSaves), feed(feed)
{
}
```

- [ ] **Step 4: Construct HttpCloudSaveClient in main and inject it**

In `source/main.cpp`, add the include near the other feed/platform includes:

```cpp
#include "platform/saves/http_cloud_save_client.hpp"
```

After `feedClient` is constructed (it reuses `apiBaseUrl` and `httpClient`), add:

```cpp
    // Cloud saves: real HTTP client against the same thomaz-api base URL. The
    // access token is read from auth_store per call by the Save Detail screen.
    auto cloudSaveClient = std::make_unique<thomaz::HttpCloudSaveClient>(
        httpClient.get(), apiBaseUrl);
```

Update the `HomeActivity` construction to pass it:

```cpp
    brls::Application::pushActivity(
        new thomaz::HomeActivity(titleService.get(), httpClient.get(), saveService.get(),
                                 feedClient.get(), albumSource.get(), cloudSaveClient.get()));
```

- [ ] **Step 5: Build + smoke run**

Run: `./scripts/build-desktop.sh && timeout 8 ./build_desktop/thomaz; echo "RUN EXIT: $?"`
Expected: `BUILD EXIT: 0`, then `RUN EXIT: 124` (healthy — still running when killed).

- [ ] **Step 6: Commit**

```bash
git add source/app/home_activity.hpp source/app/home_activity.cpp source/app/save_manager_activity.hpp source/app/save_manager_activity.cpp source/app/save_detail_activity.hpp source/app/save_detail_activity.cpp source/main.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(saves): inject ICloudSaveClient + IFeedClient into Save Detail

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 11: Cloud section UI — status on open + login gate

**Files:**
- Modify: `resources/xml/activity/save_detail.xml`
- Modify: `resources/i18n/en-US/thomaz.json`, `resources/i18n/pt-BR/thomaz.json`
- Modify: `source/app/save_detail_activity.hpp`, `source/app/save_detail_activity.cpp`

- [ ] **Step 1: Add the Cloud section to the XML**

In `resources/xml/activity/save_detail.xml`, insert this block **after** the `backupButton` Box and **before** the `history` Label:

```xml
        <!-- Cloud sync section -->
        <brls:Label text="@i18n/thomaz/saves/cloud_section"
                    fontSize="15" textColor="@theme/thomaz/text_dim" marginBottom="6"/>
        <brls:Label id="cloudStatus" text="" fontSize="14"
                    textColor="@theme/thomaz/text_dim" marginBottom="8"/>
        <brls:Box id="cloudButtons" axis="row" marginBottom="20" visibility="gone">
            <brls:Box id="cloudUpload" axis="row" alignItems="center" justifyContent="center"
                      width="180" height="48" cornerRadius="12" focusable="true"
                      backgroundColor="@theme/thomaz/tile_saves" marginRight="12">
                <brls:Label text="@i18n/thomaz/saves/cloud_upload" fontSize="16" textColor="#FFFFFF"/>
            </brls:Box>
            <brls:Box id="cloudDownload" axis="row" alignItems="center" justifyContent="center"
                      width="180" height="48" cornerRadius="12" focusable="true"
                      backgroundColor="@theme/thomaz/surface_2">
                <brls:Label text="@i18n/thomaz/saves/cloud_download" fontSize="16"
                            textColor="@theme/thomaz/text"/>
            </brls:Box>
        </brls:Box>
        <brls:Box id="cloudLogin" axis="row" alignItems="center" justifyContent="center"
                  width="220" height="48" cornerRadius="12" focusable="true"
                  backgroundColor="@theme/thomaz/surface_2" marginBottom="20" visibility="gone">
            <brls:Label text="@i18n/thomaz/saves/cloud_login" fontSize="16"
                        textColor="@theme/thomaz/text"/>
        </brls:Box>
```

> If `@theme/thomaz/surface_2` is not a registered theme color, use the literal `#22242D` for the secondary button backgrounds (the codebase uses `nvgRGB(0x22,0x24,0x2D)` as "surface_2"). Verify the theme keys in `source/app/theme.cpp` and adjust before building.

- [ ] **Step 2: Add i18n strings (en-US)**

In `resources/i18n/en-US/thomaz.json`, inside the `"saves"` object, add:

```json
        "cloud_section": "Cloud",
        "cloud_upload": "Upload to cloud",
        "cloud_download": "Download from cloud",
        "cloud_login": "Log in to use cloud saves",
        "cloud_status_none": "Nothing in the cloud yet",
        "cloud_status_synced": "Cloud: rev {{n}}",
        "cloud_status_ahead": "Cloud is newer than local (rev {{n}})",
        "cloud_status_loading": "Checking the cloud…",
        "cloud_status_error": "Couldn't reach the cloud",
        "cloud_uploading": "Uploading…",
        "cloud_downloading": "Downloading…",
        "cloud_upload_ok": "Uploaded to cloud",
        "cloud_download_ok": "Downloaded from cloud",
        "cloud_err_auth": "Session expired — log in again",
        "cloud_err_toobig": "Save is too large for the cloud",
        "cloud_err_network": "No connection to the cloud",
        "cloud_err_generic": "Cloud failed, try again",
        "cloud_conflict_body": "The cloud changed elsewhere. Keep the cloud copy or send yours?",
        "cloud_keep_cloud": "Keep cloud",
        "cloud_send_mine": "Send mine",
        "cloud_restore_q": "Downloaded. Restore it into the game now?"
```

- [ ] **Step 3: Add i18n strings (pt-BR)**

In `resources/i18n/pt-BR/thomaz.json`, inside the `"saves"` object, add:

```json
        "cloud_section": "Nuvem",
        "cloud_upload": "Enviar pra nuvem",
        "cloud_download": "Baixar da nuvem",
        "cloud_login": "Faça login pra usar a nuvem",
        "cloud_status_none": "Nada na nuvem ainda",
        "cloud_status_synced": "Nuvem: rev {{n}}",
        "cloud_status_ahead": "Nuvem mais nova que o local (rev {{n}})",
        "cloud_status_loading": "Consultando a nuvem…",
        "cloud_status_error": "Não consegui falar com a nuvem",
        "cloud_uploading": "Enviando…",
        "cloud_downloading": "Baixando…",
        "cloud_upload_ok": "Enviado pra nuvem",
        "cloud_download_ok": "Baixado da nuvem",
        "cloud_err_auth": "Sessão expirada — faça login de novo",
        "cloud_err_toobig": "Save grande demais pra nuvem",
        "cloud_err_network": "Sem conexão com a nuvem",
        "cloud_err_generic": "Falha na nuvem, tente de novo",
        "cloud_conflict_body": "A nuvem mudou em outro lugar. Manter o da nuvem ou enviar o seu?",
        "cloud_keep_cloud": "Manter da nuvem",
        "cloud_send_mine": "Enviar o meu",
        "cloud_restore_q": "Baixado. Restaurar no jogo agora?"
```

- [ ] **Step 4: Declare the cloud helpers + state in the header**

In `source/app/save_detail_activity.hpp`, add `#include <string>` and `#include <vector>` if absent, and add to the private section:

```cpp
    void refreshCloudStatus();              // GET status on open (if logged in)
    void showCloudLoggedOut();              // show the login button, hide actions
    bool requireSession();                  // true if logged in; else prompt login
    void setCloudStatusText(const std::string& text);
    std::string cloudErrorText(const std::string& apiError) const;

    int cloudRevision = 0;                  // last revision seen from the cloud
    bool cloudBusy = false;
```

- [ ] **Step 5: Implement the open/status/login logic**

In `source/app/save_detail_activity.cpp`, add includes near the top:

```cpp
#include "app/auth_activity.hpp"
#include "platform/feed/auth_store.hpp"
#include "platform/saves/sync_store.hpp"
#include "core/saves/save_sync.hpp"
```

At the end of `onContentAvailable()` (after `this->refreshHistory();`), wire the buttons and kick off the status fetch:

```cpp
    if (auto* up = this->getView("cloudUpload")) {
        up->registerClickAction([this](brls::View*) { this->doUpload(); return true; });
        up->addGestureRecognizer(new brls::TapGestureRecognizer(up));
    }
    if (auto* down = this->getView("cloudDownload")) {
        down->registerClickAction([this](brls::View*) { this->doDownload(); return true; });
        down->addGestureRecognizer(new brls::TapGestureRecognizer(down));
    }
    if (auto* login = this->getView("cloudLogin")) {
        login->registerClickAction([this](brls::View*) { this->requireSession(); return true; });
        login->addGestureRecognizer(new brls::TapGestureRecognizer(login));
    }

    if (load_session().has_value())
        this->refreshCloudStatus();
    else
        this->showCloudLoggedOut();
```

> `doUpload()` and `doDownload()` are added in Tasks 12 and 13. To keep this task's build green, add **temporary stubs** now in the .cpp (they will be replaced):
> ```cpp
> void SaveDetailActivity::doUpload()   { /* Task 12 */ }
> void SaveDetailActivity::doDownload() { /* Task 13 */ }
> ```
> and declare `void doUpload(); void doDownload();` in the header's private section.

Add the helper implementations before the closing namespace brace:

```cpp
void SaveDetailActivity::setCloudStatusText(const std::string& text) {
    if (auto* lbl = (brls::Label*)this->getView("cloudStatus"))
        lbl->setText(text);
}

void SaveDetailActivity::showCloudLoggedOut() {
    if (auto* btns = this->getView("cloudButtons"))
        btns->setVisibility(brls::Visibility::GONE);
    if (auto* login = this->getView("cloudLogin"))
        login->setVisibility(brls::Visibility::VISIBLE);
    this->setCloudStatusText("");
}

bool SaveDetailActivity::requireSession() {
    if (load_session().has_value())
        return true;
    // Reuse the feed's auth screen; on success, refresh the cloud status.
    brls::Application::pushActivity(new AuthActivity(this->feed, [this]() {
        if (this->alive->load()) {
            if (auto* login = this->getView("cloudLogin"))
                login->setVisibility(brls::Visibility::GONE);
            if (auto* btns = this->getView("cloudButtons"))
                btns->setVisibility(brls::Visibility::VISIBLE);
            this->refreshCloudStatus();
        }
    }));
    return false;
}

std::string SaveDetailActivity::cloudErrorText(const std::string& apiError) const {
    if (apiError == kCloudAuthExpired) return "thomaz/saves/cloud_err_auth"_i18n;
    if (apiError == "save_too_large")  return "thomaz/saves/cloud_err_toobig"_i18n;
    if (apiError.rfind("http_", 0) == 0) return "thomaz/saves/cloud_err_generic"_i18n;
    if (apiError.empty())              return "thomaz/saves/cloud_err_network"_i18n;
    return "thomaz/saves/cloud_err_generic"_i18n;
}

void SaveDetailActivity::refreshCloudStatus() {
    if (auto* btns = this->getView("cloudButtons"))
        btns->setVisibility(brls::Visibility::VISIBLE);
    if (auto* login = this->getView("cloudLogin"))
        login->setVisibility(brls::Visibility::GONE);
    this->setCloudStatusText("thomaz/saves/cloud_status_loading"_i18n);

    auto sess = load_session();
    std::string token = sess ? sess->token : "";
    ICloudSaveClient* c = this->cloudSaves;
    std::uint64_t tid   = this->title.title_id;
    auto alive          = this->alive;

    brls::async([this, c, alive, token, tid]() {
        CloudStatus s = c->getStatus(token, tid);
        brls::sync([this, alive, s, tid]() {
            if (!alive->load()) return;
            if (!s.ok) {
                if (s.error == kCloudAuthExpired) { this->showCloudLoggedOut(); }
                this->setCloudStatusText(this->cloudErrorText(s.error));
                return;
            }
            this->cloudRevision = s.revision;
            int synced = load_synced_revision(tid);
            core::SyncSituation sit = core::classify(s.exists, s.revision, synced);
            std::string text;
            if (sit == core::SyncSituation::NoCloud) {
                text = "thomaz/saves/cloud_status_none"_i18n;
            } else {
                std::string key = (sit == core::SyncSituation::CloudAhead)
                                      ? "thomaz/saves/cloud_status_ahead"_i18n
                                      : "thomaz/saves/cloud_status_synced"_i18n;
                auto pos = key.find("{{n}}");
                if (pos != std::string::npos)
                    key.replace(pos, 5, std::to_string(s.revision));
                text = key;
            }
            this->setCloudStatusText(text);
        });
    });
}
```

- [ ] **Step 6: Build + smoke run**

Run: `./scripts/build-desktop.sh && timeout 8 ./build_desktop/thomaz; echo "RUN EXIT: $?"`
Expected: `BUILD EXIT: 0`, `RUN EXIT: 124`. (On desktop the fake client reports no cloud slot, so the status reads "Nothing in the cloud yet".)

- [ ] **Step 7: Commit**

```bash
git add resources/xml/activity/save_detail.xml resources/i18n/en-US/thomaz.json resources/i18n/pt-BR/thomaz.json source/app/save_detail_activity.hpp source/app/save_detail_activity.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(saves): cloud section UI — status on open + login gate

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 12: Upload (push) flow + conflict dialog

**Files:**
- Modify: `source/app/save_detail_activity.cpp`

- [ ] **Step 1: Replace the doUpload stub**

In `source/app/save_detail_activity.cpp`, replace the `doUpload` stub with the real flow. It re-checks cloud status, applies the push plan, and on conflict shows a dialog:

```cpp
void SaveDetailActivity::doUpload() {
    if (this->cloudBusy) return;
    if (!this->requireSession()) return;
    this->cloudBusy = true;
    this->setCloudStatusText("thomaz/saves/cloud_uploading"_i18n);

    auto sess = load_session();
    std::string token = sess ? sess->token : "";
    ICloudSaveClient* c = this->cloudSaves;
    ISaveService* svc   = this->saveService;
    std::uint64_t tid   = this->title.title_id;
    std::string label   = this->title.name;
    auto alive          = this->alive;

    brls::async([this, c, svc, alive, token, tid, label]() {
        // Fresh status to decide conflict vs clean push.
        CloudStatus s = c->getStatus(token, tid);
        if (!s.ok) {
            std::string err = s.error;
            brls::sync([this, alive, err]() {
                if (!alive->load()) return;
                this->cloudBusy = false;
                this->setCloudStatusText(this->cloudErrorText(err));
            });
            return;
        }
        int synced = load_synced_revision(tid);
        core::SyncSituation sit = core::classify(s.exists, s.revision, synced);
        core::PushPlan plan = core::plan_push(sit, s.revision);

        brls::sync([this, alive, plan, tid, token, c, svc, label]() {
            if (!alive->load()) return;
            if (plan.isConflict) {
                this->cloudBusy = false;
                // Ask: keep cloud (download) or send mine (force push at this revision).
                brls::Dialog* dlg = new brls::Dialog("thomaz/saves/cloud_conflict_body"_i18n);
                int rev = plan.revision;
                dlg->addButton("thomaz/saves/cloud_send_mine"_i18n,
                               [this, rev]() { this->pushAtRevision(rev); });
                dlg->addButton("thomaz/saves/cloud_keep_cloud"_i18n,
                               [this]() { this->doDownload(); });
                dlg->open();
                return;
            }
            // Clean push: keep busy=true and go straight to the upload.
            this->pushAtRevision(plan.revision);
        });
    });
}
```

- [ ] **Step 2: Add the pushAtRevision helper (declare in header, implement in .cpp)**

Declare in the header private section: `void pushAtRevision(int revision);`

Implement in the .cpp:

```cpp
void SaveDetailActivity::pushAtRevision(int revision) {
    this->cloudBusy = true;
    this->setCloudStatusText("thomaz/saves/cloud_uploading"_i18n);

    auto sess = load_session();
    std::string token = sess ? sess->token : "";
    ICloudSaveClient* c = this->cloudSaves;
    ISaveService* svc   = this->saveService;
    std::uint64_t tid   = this->title.title_id;
    std::string label   = this->title.name;
    auto alive          = this->alive;

    brls::async([this, c, svc, alive, token, tid, label, revision]() {
        std::string err;
        std::vector<std::uint8_t> blob = svc->packageActiveSave(tid, &err);
        if (blob.empty()) {
            brls::sync([this, alive, err]() {
                if (!alive->load()) return;
                this->cloudBusy = false;
                this->setCloudStatusText("thomaz/saves/cloud_err_generic"_i18n);
                brls::Application::notify("thomaz/saves/cloud_err_generic"_i18n);
            });
            return;
        }
        CloudPush r = c->push(token, tid, blob, label, revision);
        brls::sync([this, alive, r, tid]() {
            if (!alive->load()) return;
            this->cloudBusy = false;
            if (r.ok) {
                save_synced_revision(tid, r.newRevision);
                brls::Application::notify("thomaz/saves/cloud_upload_ok"_i18n);
                this->refreshCloudStatus();
                return;
            }
            if (r.conflict) {
                // Lost a race: re-run the decision so the user is asked again.
                this->doUpload();
                return;
            }
            this->setCloudStatusText(this->cloudErrorText(r.error));
            brls::Application::notify(this->cloudErrorText(r.error));
        });
    });
}
```

- [ ] **Step 3: Build + smoke run**

Run: `./scripts/build-desktop.sh && timeout 8 ./build_desktop/thomaz; echo "RUN EXIT: $?"`
Expected: `BUILD EXIT: 0`, `RUN EXIT: 124`.

- [ ] **Step 4: Commit**

```bash
git add source/app/save_detail_activity.cpp source/app/save_detail_activity.hpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(saves): cloud upload flow with conflict dialog

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 13: Download (pull) flow + restore prompt

**Files:**
- Modify: `source/app/save_detail_activity.cpp`

- [ ] **Step 1: Replace the doDownload stub**

In `source/app/save_detail_activity.cpp`, replace the `doDownload` stub. It pulls the blob, imports it as a local backup, persists the synced revision, refreshes the history, then asks whether to restore the newest backup into the game:

```cpp
void SaveDetailActivity::doDownload() {
    if (this->cloudBusy) return;
    if (!this->requireSession()) return;
    this->cloudBusy = true;
    this->setCloudStatusText("thomaz/saves/cloud_downloading"_i18n);

    auto sess = load_session();
    std::string token = sess ? sess->token : "";
    ICloudSaveClient* c = this->cloudSaves;
    ISaveService* svc   = this->saveService;
    std::uint64_t tid   = this->title.title_id;
    auto alive          = this->alive;

    brls::async([this, c, svc, alive, token, tid]() {
        CloudPull p = c->pull(token, tid);
        std::string importErr;
        bool imported = false;
        if (p.ok && p.exists)
            imported = svc->importPackageAsBackup(tid, p.blob, &importErr);

        brls::sync([this, alive, p, imported, importErr, tid]() {
            if (!alive->load()) return;
            this->cloudBusy = false;
            if (!p.ok) {
                if (p.error == kCloudAuthExpired) this->showCloudLoggedOut();
                this->setCloudStatusText(this->cloudErrorText(p.error));
                return;
            }
            if (!p.exists) {
                this->setCloudStatusText("thomaz/saves/cloud_status_none"_i18n);
                return;
            }
            if (!imported) {
                this->setCloudStatusText("thomaz/saves/cloud_err_generic"_i18n);
                brls::Application::notify(importErr.empty()
                    ? "thomaz/saves/cloud_err_generic"_i18n : importErr);
                return;
            }
            save_synced_revision(tid, p.revision);
            this->cloudRevision = p.revision;
            brls::Application::notify("thomaz/saves/cloud_download_ok"_i18n);
            this->refreshHistory();      // the new backup now shows in the list
            this->refreshCloudStatus();  // now in sync

            // Offer to restore the just-downloaded backup (newest first).
            auto entries = core::list_backups(core::saves_root(), tid);
            if (!entries.empty()) {
                core::BackupEntry newest = entries.front();
                brls::Dialog* dlg = new brls::Dialog("thomaz/saves/cloud_restore_q"_i18n);
                dlg->addButton("thomaz/saves/action_restore"_i18n,
                               [this, newest]() { this->doRestore(newest); });
                dlg->addButton("brls/hints/back"_i18n, []() {});
                dlg->open();
            }
        });
    });
}
```

- [ ] **Step 2: Build + smoke run**

Run: `./scripts/build-desktop.sh && timeout 8 ./build_desktop/thomaz; echo "RUN EXIT: $?"`
Expected: `BUILD EXIT: 0`, `RUN EXIT: 124`.

- [ ] **Step 3: Run the full test suite**

Run: `cd tests && make clean && make test`
Expected: all cases pass (existing feed/core suites + the new saves suites).

- [ ] **Step 4: Commit**

```bash
git add source/app/save_detail_activity.cpp
git -c user.name='luizfbalves' -c user.email='luizzbanndera@gmail.com' commit -m "feat(saves): cloud download flow + restore prompt

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Final Verification (after all tasks)

- [ ] `cd tests && make clean && make test` → all suites green.
- [ ] `./scripts/build-desktop.sh` → `BUILD EXIT: 0`.
- [ ] `timeout 8 ./build_desktop/thomaz; echo $?` → `124` (healthy).
- [ ] Manual desktop sanity: open Save Manager → a game → Save Detail shows the "Cloud" section. Logged out → "Log in to use cloud saves" button. (With the fake desktop client, status reads "Nothing in the cloud yet"; for a live test, point `api_url.txt` at a running `api/` instance and log in via the Feed.)
- [ ] Dispatch a final code review over the whole branch diff.

## Notes / Known limitations (carried from the spec)

- No `DELETE` of a cloud save (out of scope).
- No JWT auto-refresh in the cloud client — a `401` surfaces "Session expired" and routes to login.
- Cross-console profile mapping is not resolved; the blob carries `uid_hex` paths and restore behaves like the existing local restore.
- After a download, the app always offers to restore (rather than comparing against the active save's mtime, which isn't reliably available); the on-screen "Cloud is newer" status already signals when the cloud copy is ahead.
