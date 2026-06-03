# thomaz v1 — Phase 3: Title Service + db paths (list installed games) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** List the games installed on the console (id, name, author, version) via the libnx `ns` service, and provide the pure path/URL helpers that map a title to its switch-cheats-db resources and its on-SD cheat file.

**Architecture:** Pure logic (`source/core/db_paths`) is host-tested with doctest. The libnx-dependent `TitleService` lives behind an interface in `source/platform/`, is compiled into the `.nro` by CI, and is exercised on hardware (libnx `ns` cannot run on the host or in CI). The app logs the discovered game count at startup as an on-device sanity signal until the Phase 4 UI consumes the list.

**Tech Stack:** C++17, libnx (`ns` service), doctest (host tests), xfangfang/borealis (app), CMake/CI.

**Scope note:** The cheat **download** (CheatRepository over HTTPS) is deliberately NOT in this phase — it needs an HTTPS/TLS client (switch-curl) whose availability must be resolved separately (Borealis does not pull libcurl, and devkitPro's package servers are Cloudflare-blocked from the maintainer's network). That is **Phase 3b**, whose first task is to confirm/secure the HTTP dependency in CI.

---

## File Structure

```
thomaz/
├── source/core/
│   ├── db_paths.hpp / db_paths.cpp     # NEW pure: title-id hex, db URLs, SD cheat path
├── source/platform/
│   ├── title.hpp                       # NEW: InstalledTitle struct + ITitleService interface (pure header)
│   ├── title_service_switch.hpp        # NEW: NsTitleService declaration
│   └── title_service_switch.cpp        # NEW: libnx ns implementation (compiled only in the Switch/app build)
├── source/main.cpp                     # MODIFIED: log installed-game count at startup
└── tests/
    └── test_db_paths.cpp               # NEW host tests
```

**Responsibilities:**
- `db_paths` — pure string mapping only (title id ↔ hex, URLs, SD path). Host-tested.
- `title.hpp` — the data type + interface the rest of the app depends on (no libnx). Keeps UI/logic decoupled from the `ns` implementation.
- `title_service_switch.*` — the only file that includes `<switch.h>`. Lives in `source/platform/`, so the host test Makefile (which only globs `source/core/*.cpp`) never compiles it.

---

## Task 1: db_paths — pure title/URL/path helpers (host TDD)

**Files:**
- Create: `source/core/db_paths.hpp`, `source/core/db_paths.cpp`
- Create: `tests/test_db_paths.cpp`

- [ ] **Step 1: Declare the helpers.** Create `source/core/db_paths.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <string>

namespace thomaz::core {

// 16-char hex of a Switch title id. upper=false -> lowercase (SD path form),
// upper=true -> uppercase (switch-cheats-db filename form).
std::string title_id_hex(std::uint64_t title_id, bool upper);

// switch-cheats-db raw URLs (per-title JSON).
std::string cheats_url(std::uint64_t title_id);    // .../cheats/<UPPER>.json
std::string versions_url(std::uint64_t title_id);  // .../versions/<UPPER>.json

// On-SD Atmosphère cheat file path for a resolved build id.
// build_id is used verbatim (already uppercase hex from the db).
std::string sd_cheat_path(std::uint64_t title_id, const std::string& build_id);

} // namespace thomaz::core
```

- [ ] **Step 2: Write failing tests.** Create `tests/test_db_paths.cpp`:

```cpp
#include "doctest.h"
#include "core/db_paths.hpp"

using namespace thomaz::core;

TEST_CASE("title_id_hex pads to 16 chars, both cases") {
    // Super Mario Odyssey
    CHECK(title_id_hex(0x0100000000010000ULL, false) == "0100000000010000");
    CHECK(title_id_hex(0x0100000000010000ULL, true)  == "0100000000010000");
    // A value with hex letters exercises upper/lower
    CHECK(title_id_hex(0x01006A800016E000ULL, false) == "01006a800016e000");
    CHECK(title_id_hex(0x01006A800016E000ULL, true)  == "01006A800016E000");
    // Small value still pads to 16
    CHECK(title_id_hex(0x1ULL, false) == "0000000000000001");
}

TEST_CASE("db URLs use the uppercase title id") {
    CHECK(cheats_url(0x01006A800016E000ULL) ==
        "https://raw.githubusercontent.com/HamletDuFromage/switch-cheats-db/master/cheats/01006A800016E000.json");
    CHECK(versions_url(0x0100000000010000ULL) ==
        "https://raw.githubusercontent.com/HamletDuFromage/switch-cheats-db/master/versions/0100000000010000.json");
}

TEST_CASE("sd_cheat_path uses lowercase title id and verbatim build id") {
    CHECK(sd_cheat_path(0x0100000000010000ULL, "B424BE150A8E7D78") ==
        "/atmosphere/contents/0100000000010000/cheats/B424BE150A8E7D78.txt");
}
```

- [ ] **Step 3: Run — expect FAIL (db_paths.cpp missing)**

Run:
```bash
make -C tests test
```
Expected: undefined references to the db_paths functions.

- [ ] **Step 4: Implement.** Create `source/core/db_paths.cpp`:

```cpp
#include "core/db_paths.hpp"
#include <array>

namespace thomaz::core {

std::string title_id_hex(std::uint64_t title_id, bool upper) {
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[i] = digits[title_id & 0xF];
        title_id >>= 4;
    }
    return out;
}

namespace {
const std::string DB_BASE =
    "https://raw.githubusercontent.com/HamletDuFromage/switch-cheats-db/master";
}

std::string cheats_url(std::uint64_t title_id) {
    return DB_BASE + "/cheats/" + title_id_hex(title_id, true) + ".json";
}

std::string versions_url(std::uint64_t title_id) {
    return DB_BASE + "/versions/" + title_id_hex(title_id, true) + ".json";
}

std::string sd_cheat_path(std::uint64_t title_id, const std::string& build_id) {
    return "/atmosphere/contents/" + title_id_hex(title_id, false) +
           "/cheats/" + build_id + ".txt";
}

} // namespace thomaz::core
```

- [ ] **Step 5: Run — expect PASS**

Run: `make -C tests test`
Expected: all db_paths tests pass.

- [ ] **Step 6: Commit**

```bash
git add source/core/db_paths.hpp source/core/db_paths.cpp tests/test_db_paths.cpp
git commit -m "feat(core): db_paths — title-id hex, switch-cheats-db URLs, SD cheat path"
```

---

## Task 2: InstalledTitle + ITitleService interface

**Files:**
- Create: `source/platform/title.hpp`

- [ ] **Step 1: Define the data type and interface.** Create `source/platform/title.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace thomaz {

// One installed application, as the UI needs to display and act on it.
struct InstalledTitle {
    std::uint64_t title_id = 0;
    std::string name;     // localized display name (from NACP)
    std::string author;   // publisher (from NACP)
    std::uint32_t version = 0; // installed application version (maps to build_id via the db)
};

// Lists installed titles. Implemented on Switch by NsTitleService; the
// interface lets the UI and tests depend on behavior, not on libnx.
class ITitleService {
  public:
    virtual ~ITitleService() = default;
    virtual std::vector<InstalledTitle> listInstalled() = 0;
};

} // namespace thomaz
```

- [ ] **Step 2: Commit** (header-only, no test needed — it is a pure declaration consumed by Task 3)

```bash
git add source/platform/title.hpp
git commit -m "feat(platform): InstalledTitle struct + ITitleService interface"
```

---

## Task 3: NsTitleService — libnx `ns` implementation

**Files:**
- Create: `source/platform/title_service_switch.hpp`, `source/platform/title_service_switch.cpp`

This file is the only one that includes `<switch.h>`. It is compiled into the `.nro` by CMake (CI) and runs on hardware. It cannot be unit-tested on the host; verification = CI compile + on-device behavior.

- [ ] **Step 1: Declare.** Create `source/platform/title_service_switch.hpp`:

```cpp
#pragma once
#include "platform/title.hpp"

namespace thomaz {

// libnx ns-backed implementation. Construct after the app starts; it manages
// nsInitialize/nsExit via init()/exit().
class NsTitleService : public ITitleService {
  public:
    bool init();   // nsInitialize; returns true on success
    void exit();   // nsExit
    std::vector<InstalledTitle> listInstalled() override;
};

} // namespace thomaz
```

- [ ] **Step 2: Implement.** Create `source/platform/title_service_switch.cpp`:

```cpp
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
```

- [ ] **Step 3: Verify it compiles (CI).** This cannot build on the host (needs libnx). Push and let the Switch CI build it. Until Task 4 references it, add it to the build by confirming CMake's `file(GLOB_RECURSE source/*.cpp)` picks it up (it does — it lives under `source/platform/`). Commit, then the CI run in Task 4 confirms compilation.

```bash
git add source/platform/title_service_switch.hpp source/platform/title_service_switch.cpp
git commit -m "feat(platform): NsTitleService — list installed titles via libnx ns"
```

---

## Task 4: Wire the service into the app + verify CI green

**Files:**
- Modify: `source/main.cpp`

- [ ] **Step 1: Call the service at startup and log the count.** In `source/main.cpp`, add the include and, after `brls::Application::init()` succeeds (before `createWindow`), insert the diagnostic:

Add near the other includes:
```cpp
#include "platform/title_service_switch.hpp"
```

After the `init()` success check, add:
```cpp
    // Phase 3 sanity signal (replaced by the bento/list UI in Phase 4):
    // list installed games and log how many were found.
    {
        thomaz::NsTitleService titleService;
        if (titleService.init()) {
            auto titles = titleService.listInstalled();
            brls::Logger::info("thomaz: found {} installed titles", titles.size());
            titleService.exit();
        } else {
            brls::Logger::error("thomaz: failed to initialize ns title service");
        }
    }
```

- [ ] **Step 2: Push and verify the Switch CI build is GREEN**

```bash
git add source/main.cpp
git commit -m "feat(app): list installed titles at startup (Phase 3 sanity log)"
git push origin phase1-foundation
```
Then watch the `build` workflow run to completion. Expected: `Build thomaz.nro` succeeds and the `thomaz-nro` artifact uploads. If compilation fails, fix the libnx usage (field/function names) per the CI error and repeat.

- [ ] **Step 3: Confirm host tests still green**

Run: `make -C tests test`
Expected: all tests pass (db_paths + Phase 2 core), unaffected by the libnx additions.

---

## Phase 3 done = definition

- `make -C tests test` green, now including `db_paths` (title-id hex, db URLs, SD cheat path) verified against real title ids.
- `thomaz.nro` builds GREEN in CI with `NsTitleService` compiled in.
- On hardware, launching the app logs the number of installed titles (on-device confirmation the `ns` listing works).
- `ITitleService` interface is in place for the Phase 4 UI to consume.

---

## Self-Review (against spec + spike)

**Coverage:** "list installed games via ns (name/icon/version)" (spec §4 TitleService) → Tasks 2–4 ✓ (icon extraction deferred to Phase 4 UI, where it is consumed — noted, not silently dropped); version→build_id inputs (db URLs, SD path) → Task 1 ✓ using the spike's verified path templates. The 500ms/title control-data cost (spec §6) is acknowledged; a cache is a Phase 4 concern when the list is rendered repeatedly.

**Placeholder scan:** No TBD/TODO. The startup log in Task 4 is an intentional, real on-device diagnostic (removed when the Phase 4 UI consumes the list), not a placeholder.

**Type/name consistency:** `thomaz::core` for pure helpers; `thomaz::InstalledTitle` / `thomaz::ITitleService` / `thomaz::NsTitleService` consistent across `title.hpp`, `title_service_switch.*`, and `main.cpp`. `db_paths` names (`title_id_hex`, `cheats_url`, `versions_url`, `sd_cheat_path`) match between header, impl, and tests. libnx symbols (`nsListApplicationRecord`, `nsGetApplicationControlData`, `nsListApplicationContentMetaStatus`, `nacpGetLanguageEntry`, `NsApplicationControlSource_Storage`, `NcmContentMetaType_Application`) are the documented libnx `ns`/`nacp` API; any mismatch surfaces at CI compile and is fixed there.
