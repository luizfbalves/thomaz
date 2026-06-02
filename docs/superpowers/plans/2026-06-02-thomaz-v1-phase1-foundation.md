# thomaz v1 — Phase 1: Foundation & De-risking — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stand up a buildable, bootable `thomaz.nro` skeleton on Borealis with a working host-side test harness and i18n, and de-risk the two unknowns (Atmosphère cheat file format + switch-cheats-db version→build_id mapping) before writing the cheat logic.

**Architecture:** Switch homebrew NRO built with devkitPro/libnx. UI via the Borealis library (git submodule, `borealis.mk` Makefile include, romfs assets). Pure C++ logic (later phases) is compiled and tested on the host PC with the single-header `doctest` framework, so business logic is verifiable without a console. This phase produces the skeleton + a research findings document that unblocks the detailed Phase 2–5 plans.

**Tech Stack:** devkitPro (devkitA64, libnx), Borealis (`switch-glfw`, `switch-mesa`, `switch-glm`), C++17, doctest (host tests), git submodules.

---

## Why this phase exists (read first)

The design spec (`docs/superpowers/specs/2026-06-02-thomaz-switch-homebrew-design.md`) flags two unknowns that make line-exact code impossible to write right now:

1. **Borealis API** — the library README states its wiki documents only the *old* version; `main` is WIP. The current `brls::Application` / Activity / i18n API must be read from the in-repo `demo/` rather than invented.
2. **Cheat data formats** — the exact Atmosphère cheat `.txt` text format and the switch-cheats-db `version → build_id` mapping (spec §7, the project's "go/no-go" risk) are unverified.

Writing the parser, repository, and UI code before these are confirmed would require placeholders/guesses, which the writing-plans skill forbids. Therefore Phase 1 = scaffold what *is* concrete + run the spike. **Phases 2–5 are planned in detail after Task 5 lands** (see roadmap at the end).

---

## File Structure (created in this phase)

```
thomaz/
├── Makefile                         # Switch NRO build (includes borealis.mk)
├── .gitmodules                      # borealis submodule
├── lib/
│   ├── borealis/                    # submodule: natinusala/borealis (main)
│   └── doctest/doctest.h            # vendored single-header test framework
├── resources/                       # romfs root (copied from borealis + our assets)
│   └── i18n/
│       ├── en-US/app.json           # English strings
│       └── pt-BR/app.json           # Portuguese strings
├── source/
│   └── main.cpp                     # app entry (adapted from borealis demo)
├── tests/
│   ├── Makefile                     # host build (system g++), NOT devkitPro
│   ├── test_main.cpp                # doctest entry point
│   └── test_smoke.cpp               # trivial test proving the harness runs
└── docs/superpowers/research/
    └── 2026-06-02-cheat-format-and-db-spike.md   # Task 5 deliverable
```

**Responsibilities:**
- `Makefile` — only knows how to build the Switch NRO via Borealis.
- `tests/Makefile` — host-only; compiles pure logic + doctest with the system compiler. Kept entirely separate from the devkitPro build so logic stays testable on the PC.
- `source/main.cpp` — entry point only; real screens come in later phases.
- `resources/i18n/*` — all user-facing strings; no hardcoded UI text anywhere else.

---

## Task 1: Install and verify the devkitPro toolchain

**Files:** none (environment setup).

> Note: `/opt/devkitpro` does not exist yet in this environment. The `sudo` steps below likely need to be run by the user. If a step needs sudo and the agent cannot run it, ask the user to run it from the prompt with a leading `!` (e.g. `! sudo ./install-devkitpro-pacman`).

- [ ] **Step 1: Download the devkitPro pacman installer**

Run:
```bash
cd /tmp
wget https://apt.devkitpro.org/install-devkitpro-pacman
chmod +x install-devkitpro-pacman
```
Expected: file `/tmp/install-devkitpro-pacman` exists and is executable.

- [ ] **Step 2: Install devkitPro pacman (user runs sudo)**

Run:
```bash
sudo /tmp/install-devkitpro-pacman
```
Expected: installs to `/opt/devkitpro`; adds env vars via `/etc/profile.d/devkit-env.sh`.

- [ ] **Step 3: Load the devkitPro environment in this shell**

Run:
```bash
source /etc/profile.d/devkit-env.sh
echo "$DEVKITPRO"
```
Expected: prints `/opt/devkitpro`.

- [ ] **Step 4: Install Switch dev packages + Borealis dependencies**

Run:
```bash
sudo dkp-pacman -S --noconfirm switch-dev switch-glfw switch-mesa switch-glm
```
Expected: packages install without error.

- [ ] **Step 5: Verify the cross-compiler is on PATH**

Run:
```bash
"$DEVKITPRO/devkitA64/bin/aarch64-none-elf-g++" --version
```
Expected: prints a GCC version banner (e.g. `aarch64-none-elf-g++ (devkitA64) 14.x`).

(No commit — environment only.)

---

## Task 2: Scaffold a bootable thomaz NRO from the Borealis demo

**Files:**
- Create: `.gitmodules`, `lib/borealis/` (submodule)
- Create: `Makefile`
- Create: `source/main.cpp`
- Create: `resources/` (romfs root)

- [ ] **Step 1: Add Borealis as a submodule**

Run:
```bash
git submodule add https://github.com/natinusala/borealis.git lib/borealis
git submodule update --init --recursive
```
Expected: `lib/borealis/` populated; `lib/borealis/demo/` and `lib/borealis/library/borealis.mk` exist.

- [ ] **Step 2: Learn the current API from the demo (no guessing)**

Read these files and note the exact current API used:
```bash
cat lib/borealis/demo/main.cpp
cat lib/borealis/demo/Makefile
ls lib/borealis/demo/resources/i18n
```
Record the exact calls used for: app init (e.g. `brls::Application::init`), setting the window/app title, creating/pushing the root Activity or View, and string lookup (e.g. `brls::getStr` / `"@i18n/..."`). These exact symbols are the source of truth for Steps 3–4 and for Task 4.

- [ ] **Step 3: Create the project Makefile from the demo Makefile**

Copy `lib/borealis/demo/Makefile` to `./Makefile`, then change only these values:
- `TARGET` / `APP_TITLE` → `thomaz`
- `APP_AUTHOR` → `luizfbalves`
- `BOREALIS_PATH` → `lib/borealis/library` (path to where `borealis.mk` lives, relative to this Makefile)
- `ROMFS` → `resources`
- keep `OUT_SHADERS := shaders` and the `include $(BOREALIS_PATH)/borealis.mk` line intact

Run:
```bash
cp lib/borealis/demo/Makefile ./Makefile
```
Then apply the edits above. Verify the include path resolves:
```bash
test -f lib/borealis/library/borealis.mk && echo "borealis.mk found"
```
Expected: prints `borealis.mk found`.

- [ ] **Step 4: Create the romfs resources folder**

Copy the Borealis runtime resources (shaders, fonts, default i18n) so `romfs:/` paths resolve, then we will add our own i18n in Task 4.
```bash
cp -r lib/borealis/resources ./resources
ls resources
```
Expected: `resources/` contains the borealis assets (e.g. `shaders/`, `fonts/`, `i18n/`).

- [ ] **Step 5: Create `source/main.cpp` adapted from the demo**

Copy the demo entry point and reduce it to a single screen showing the app title. Use the EXACT API symbols recorded in Step 2 — do not invent calls.
```bash
cp lib/borealis/demo/main.cpp source/main.cpp
```
Then edit `source/main.cpp` so that, using the demo's own API:
- the app is initialized with title `"thomaz"`,
- the root activity/view is a single centered label reading the i18n key `app/title` (string added in Task 4; until then the demo's existing content is acceptable),
- all demo-specific tabs/example screens are removed.

The structural pattern (init → set title → create window → push root activity → main loop) comes verbatim from the demo; only the content shown is changed.

- [ ] **Step 6: Build the NRO**

Run:
```bash
source /etc/profile.d/devkit-env.sh
make -j$(nproc)
```
Expected: build completes and produces `./thomaz.nro`.

- [ ] **Step 7: Verify the build artifact**

Run:
```bash
ls -lh thomaz.nro
```
Expected: `thomaz.nro` exists, non-zero size.
Manual (hardware/Ryujinx, optional this phase): launching it shows a window titled "thomaz". Note in the commit message whether a runtime check was done.

- [ ] **Step 8: Commit**

```bash
git add .gitmodules lib/borealis Makefile source/main.cpp resources
git commit -m "feat: bootable thomaz NRO skeleton on Borealis

Builds thomaz.nro from the Borealis demo template (submodule + borealis.mk).
Single-screen skeleton; real screens land in later phases."
```

---

## Task 3: Host-side test harness (doctest)

**Files:**
- Create: `lib/doctest/doctest.h`
- Create: `tests/test_main.cpp`
- Create: `tests/test_smoke.cpp`
- Create: `tests/Makefile`

This harness compiles with the **system** g++ (not devkitPro) so pure logic modules (Phase 2: cheat parser, toggle logic, version matching) are testable on the PC. Proving it works now de-risks the whole TDD path.

- [ ] **Step 1: Vendor doctest**

Run:
```bash
mkdir -p lib/doctest
wget -O lib/doctest/doctest.h https://raw.githubusercontent.com/doctest/doctest/master/doctest/doctest.h
test -s lib/doctest/doctest.h && echo "doctest vendored"
```
Expected: prints `doctest vendored`.

- [ ] **Step 2: Write the doctest entry point**

Create `tests/test_main.cpp`:
```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
```

- [ ] **Step 3: Write a failing smoke test**

Create `tests/test_smoke.cpp`:
```cpp
#include "doctest.h"

// Proves the host test harness compiles and runs.
// Replaced by real logic tests in Phase 2.
TEST_CASE("harness runs") {
    CHECK(1 + 1 == 3); // intentionally wrong first
}
```

- [ ] **Step 4: Write the host Makefile**

Create `tests/Makefile`:
```make
CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -I../lib/doctest
SRCS     := test_main.cpp test_smoke.cpp
BIN      := run

$(BIN): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(BIN)

.PHONY: test clean
test: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)
```

- [ ] **Step 5: Run the test and verify it FAILS**

Run:
```bash
make -C tests test
```
Expected: compiles, runs, reports `1 + 1 == 3` FAILED (1 test, 1 failure). This proves failures are actually detected.

- [ ] **Step 6: Fix the assertion to pass**

Edit `tests/test_smoke.cpp`, change `CHECK(1 + 1 == 3);` to:
```cpp
    CHECK(1 + 1 == 2);
```

- [ ] **Step 7: Run the test and verify it PASSES**

Run:
```bash
make -C tests test
```
Expected: `1 test cases ... 1 assertion ... 0 failed`.

- [ ] **Step 8: Ignore the host build artifact and commit**

Run:
```bash
echo "tests/run" >> .gitignore
git add lib/doctest/doctest.h tests/test_main.cpp tests/test_smoke.cpp tests/Makefile .gitignore
git commit -m "test: add host-side doctest harness

System-g++ build for pure C++ logic, independent of devkitPro, so
business logic is testable on the host before hardware."
```

---

## Task 4: Internationalization bootstrap (PT-BR + EN)

**Files:**
- Create: `resources/i18n/en-US/app.json`
- Create: `resources/i18n/pt-BR/app.json`
- Modify: `source/main.cpp` (use the `app/title` string key)

Borealis loads strings from `romfs:/i18n/<locale>/<file>.json` and selects the locale from the console's system language. We provide both locales; the device language picks the default automatically.

- [ ] **Step 1: Create the English strings**

Create `resources/i18n/en-US/app.json`:
```json
{
    "title": "thomaz",
    "home": {
        "greeting": "What are we doing today?"
    },
    "module": {
        "cheats": {
            "title": "Cheats",
            "subtitle": "Manage your games' cheats"
        }
    },
    "common": {
        "coming_soon": "Coming soon"
    }
}
```

- [ ] **Step 2: Create the Portuguese strings**

Create `resources/i18n/pt-BR/app.json`:
```json
{
    "title": "thomaz",
    "home": {
        "greeting": "O que vamos fazer hoje?"
    },
    "module": {
        "cheats": {
            "title": "Trapaças",
            "subtitle": "Gerencie os cheats dos seus jogos"
        }
    },
    "common": {
        "coming_soon": "Em breve"
    }
}
```

- [ ] **Step 3: Reference the i18n key in `main.cpp`**

Edit `source/main.cpp` so the centered label's text uses the string key `app/title` via the lookup API recorded in Task 2 Step 2 (e.g. `brls::getStr("app/title")` — use whatever the demo confirms). The literal string "thomaz" must NOT be hardcoded in the label; it comes from the JSON.

- [ ] **Step 4: Rebuild and verify**

Run:
```bash
source /etc/profile.d/devkit-env.sh
make -j$(nproc)
ls -lh thomaz.nro
```
Expected: rebuilds successfully; `thomaz.nro` present.
Manual (optional): on a console set to Portuguese the label resolves from `pt-BR`; on English, from `en-US`.

- [ ] **Step 5: Commit**

```bash
git add resources/i18n/en-US/app.json resources/i18n/pt-BR/app.json source/main.cpp
git commit -m "feat: i18n bootstrap (pt-BR + en-US)

All user-facing strings live in romfs i18n JSON; locale follows the
console system language. No hardcoded UI text."
```

---

## Task 5: SPIKE — confirm cheat file format + switch-cheats-db mapping

**Files:**
- Create: `docs/superpowers/research/2026-06-02-cheat-format-and-db-spike.md`

This is the project's go/no-go (spec §7). It is research, not code: read the canonical sources, fetch real samples, and write down the exact formats so Phase 2 can be planned without guessing.

- [ ] **Step 1: Document the Atmosphère cheat `.txt` format**

Read `https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/features/cheats.md` and one real cheat file from the database. Capture, with concrete examples:
- The exact load path (`/atmosphere/contents/<program_id>/cheats/<build_id>.txt`).
- The textual structure: cheat section headers (e.g. `[Cheat Name]`), the master/anchor code syntax (e.g. `{Master}`), comment syntax, and the per-line opcode format.
- How `build_id` is derived (first 8 bytes of the main NSO build id, hex).
- The exact rule the parser must follow to split a file into named cheats and to re-serialize "only enabled cheats + master".

- [ ] **Step 2: Document the switch-cheats-db structure**

Read `https://github.com/HamletDuFromage/switch-cheats-db` and its releases. Capture:
- Folder layout (e.g. `contents/<title_id>/cheats/<build_id>.txt`, `versions/`).
- The release asset zips (`contents.zip`, `titles.zip`) and what each contains.
- Whether there is a per-title raw URL usable to fetch one game's cheats without downloading the whole archive.

- [ ] **Step 3: Confirm the version → build_id mapping (THE risk)**

Determine, with evidence:
- Does the db expose a mapping from a game's **version** (the value `ns` returns) to the **build_id** filename? Where (which file/zip)?
- Cross-check by picking one known title and tracing `title_id` + `version` → `build_id` → cheat file.
- Decide GO (mapping is reliably derivable from `ns` data + db) or NO-GO (needs local NSO build-id reading — Plan B in spec §7).

- [ ] **Step 4: Write the findings document**

Create `docs/superpowers/research/2026-06-02-cheat-format-and-db-spike.md` containing:
- Cheat file format (with a real annotated example).
- switch-cheats-db layout + the exact URL/zip strategy for fetching one game's cheats.
- The version→build_id finding + **GO / NO-GO verdict** with rationale.
- A short "Phase 2 inputs" section: the concrete constants/paths Phase 2 code will hardcode (load path template, db base URL, mapping file name).

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/research/2026-06-02-cheat-format-and-db-spike.md
git commit -m "docs: cheat-format + switch-cheats-db spike findings

Confirms Atmosphere cheat .txt format, db layout, and the
version->build_id mapping (go/no-go for spec §7)."
```

---

## Phase 1 done = definition

- `make` produces a bootable `thomaz.nro` from the Borealis skeleton.
- `make -C tests test` is green on the host PC.
- UI text comes from pt-BR/en-US i18n JSON, locale-selected.
- A findings doc records the exact cheat format, db layout, and a GO/NO-GO on version→build_id.

---

## Roadmap — Phases 2–5 (planned in detail AFTER Phase 1)

Each becomes its own `docs/superpowers/plans/` document once Task 5's findings make its code specifiable without placeholders.

- **Phase 2 — Cheat-format core (pure logic, TDD):** `CheatStore` parser/serializer (split file → named cheats; write only-enabled + master), and the version→build_id matching logic. Fully host-tested with doctest, using the formats confirmed in the spike.
- **Phase 3 — libnx borders:** `TitleService` (`nsListApplicationRecord` + `nsGetApplicationControlData`, icon/metadata cache) and `CheatRepository` (libcurl download from switch-cheats-db, per-title). Behind interfaces so Phase 2 logic stays mockable.
- **Phase 4 — UI (Borealis + theme):** `AppShell` + bento home + module registry; Cheats screens (game list with state badges, game detail with toggles + download, settings with language + ban warning). Touch + controller.
- **Phase 5 — Integration & hardware test:** wire the module end-to-end; manual validation on real hardware with Atmosphère (game listing, real download, actual cheat application in a game).

---

## Self-Review (against the spec)

**Spec coverage (this phase's slice):** toolchain/build (spec §9) → Tasks 1–2 ✓; i18n (§3, §4 `I18n`) → Task 4 ✓; host test strategy (§8) → Task 3 ✓; build_id risk + spike (§7) → Task 5 ✓; bento home/modules/cheat logic/libnx borders (§4, §5, §6) → explicitly deferred to Phases 2–5 with a written roadmap ✓ (no spec requirement is dropped; each maps to a named future plan).

**Placeholder scan:** No "TBD/TODO/implement later". The two "adapt from the demo" steps (Task 2 Step 5, Task 4 Step 3) are concrete actions — copy a known-good file and apply named edits — with the exact API symbols sourced in Task 2 Step 2, not invented. This is deliberate: the Borealis API is undocumented upstream, so reading-then-adapting is the correct non-guessing method.

**Type/name consistency:** i18n keys (`app/title`, `home/greeting`, `module/cheats/title`, `module/cheats/subtitle`, `common/coming_soon`) match between the JSON files and their usage references. Paths (`lib/borealis/library/borealis.mk`, `resources/`, `tests/`, `lib/doctest/doctest.h`) are consistent across tasks.
