# Native Theme Apply (Phase B) — Design

> Phase A (`docs/superpowers/specs/2026-06-04-themezer-theme-browser-design.md`)
> stops at downloading `.nxtheme` files to `sd:/themes/`. This Phase B turns
> those downloads into an installed, on-console theme: compile the differential
> `.nxtheme` into a firmware `.szs` and write it to LayeredFS so the theme takes
> effect after a reboot.

## Goal

Let a user tap **Aplicar Tema** on a downloaded theme/pack and have it actually
applied to the console (native CFW theme via LayeredFS), with the ability to
remove it, a reboot prompt, and an "applied" indicator — all inside the app.

## Decisions (locked during brainstorming)

- **Real native install.** Output is a patched `.szs` written under the CFW
  LayeredFS tree; the theme is live after reboot. Not a hand-off to another tool.
- **Engine: port exelix11's C++ core** (`SwitchThemeInjector` →
  `SwitchThemesNX/source/SwitchThemesCommon/`). It is the proven, field-tested
  pipeline (SARC, Yaz0, BNTX, BFLYT/BFLAN).
- **Relicense MIT → GPLv2.** The exelix engine is **GPLv2** (verified in the
  repo `LICENSE`, "Version 2, June 1991"). Linking it makes the combined binary
  GPLv2. The copyright holder (luizfbalves) accepts this.
- **Base layouts come pre-extracted from the SD card** (`/themes/systemData`),
  NOT extracted from firmware in-app. This drops the heaviest dependencies
  (forked hactool + mbedtls + `prod.keys`). The user dumps base layouts once
  (e.g. via NXThemes Installer, which already writes them to that folder).
- **First cut includes:** apply single themes, apply packs (all parts), remove /
  restore default, reboot prompt, active-theme indicator.
- **Active-theme tracking is our own JSON** (`/themes/.thomaz_active.json`).
  Native install writes nameless `.szs` files that carry no theme identity, so we
  persist which theme/parts we applied.

## Scope

### In scope
- Compile a downloaded `.nxtheme` (theme) or all parts of a pack into `.szs`
  using the vendored engine and base layouts from `/themes/systemData`.
- Write the `.szs` to the LayeredFS path for each target title + required marker
  files (`fsmitm.flag`, `version_hash.bin`).
- Remove the applied theme (delete the LayeredFS `lyt/` outputs) and clear our
  active-theme state.
- Persist + read active-theme state; surface an "Aplicado" badge in the browser
  grid and detail screen, and switch the detail button to **Remover Tema**.
- Reboot prompt after a successful apply/remove (`spsm` reboot-to-payload on
  Switch; no-op on desktop).
- Detect missing base layouts and explain how to generate them instead of
  failing opaquely.
- Relicense the project to GPLv2 with third-party attribution.

### Out of scope (future)
- **Path A**: in-app firmware extraction (hactool + mbedtls + `prod.keys`).
- exefs / IPS code patches (lock-screen / home-menu patches).
- Custom system fonts.
- MSBT / text patching (absent from the engine's C++ apply path anyway).
- Editing/creating themes.

## Architecture

```
ThemeDetailActivity / ThemeBrowserActivity   (UI: button state machine, dialogs, badge)
        │
        ▼
platform/themes/theme_install   ── high-level apply/remove/availability API
        │            │
        │            ├── platform/themes/cfw_paths       (CFW root, target→szs, base dir)
        │            ├── platform/themes/active_theme_store (our /themes/.thomaz_active.json)
        │            └── platform/system/reboot          (spsm reboot / desktop no-op)
        ▼
lib/switchthemes/   ── vendored exelix engine (GPLv2): SARC, Yaz0, BNTX, BFLYT/BFLAN,
                       Patcher, NXTheme container, templates  (+ stb_image, miniz)
```

### Components

#### 1. Vendored engine — `lib/switchthemes/` (GPLv2)
Port of the C++ tree from `SwitchThemesNX/source/SwitchThemesCommon/`, the
**apply** subset only:

- `SarcLib/` — SARC read/write
- `Yaz0` — self-contained (de)compress (no external zlib dependency)
- `Bntx/` — `QuickBntx`, `BRTI`, `DDS`, `DDS_conversion` (background texture
  replacement)
- `Layouts/Bflyt/` (+ pane types) and `Bflan` — binary layout/animation patching
- `Layouts/LayoutCompatibility`, `NewFirmFixes` — firmware-version heuristics
  that drop incompatible parts (surfaced as warnings) instead of crashing
- `Patcher`, `Common`, `NXTheme` (ZIP `PK` and legacy Yaz0+SARC containers),
  `Generated/` baked templates
- **Third-party single-headers vendored alongside:** `stb_image.h` (decode
  `image.jpg` / `*.png` → DDS at install time) and `miniz` (read the modern ZIP
  `.nxtheme`). `nlohmann/json` already lives at `lib/json/`.
- The on-console glue from upstream (`SwitchTools/hactool`, `RomfsCache`,
  `key_loader`, upstream `fs.cpp`) is **NOT** vendored — replaced by our own
  thin glue (below) that reads base layouts from the SD card.

The engine is treated as an upstream import: minimal edits, kept compilable on
both Switch and desktop (it is pure binary manipulation). A `lib/switchthemes/
README` records the upstream commit and the GPLv2 origin.

#### 2. `source/platform/themes/cfw_paths.{hpp,cpp}` (`namespace thomaz`)
Pure path logic, unit-testable:
- `std::string layeredfs_root()` — CFW contents root; Switch detects
  `/atmosphere/contents/` (Atmosphère ≥ 0.19 always uses `contents/`); desktop
  uses a local `themes-out/contents/` for smoke tests.
- `std::string base_layout_dir()` — `/themes/systemData` (Switch) / local on
  desktop.
- Target → title id + szs filename maps, e.g.
  `home → 0100000000001000 / ResidentMenu.szs`,
  `lock → 0100000000001000 / Entrance.szs`,
  `apps → 0100000000001000 / Flaunch.szs`,
  `set  → 0100000000001000 / Set.szs`,
  `news → 0100000000001000 / Notification.szs` (Notification = News/Notifications),
  `user → 0100000000001013 / MyPage.szs`,
  `psl  → 0100000000001007 / Psl.szs`.
- `std::string base_szs_path(target)` and `std::string output_szs_path(target)`.

#### 3. `source/platform/themes/theme_install.{hpp,cpp}` (`namespace thomaz`)
High-level orchestration over the engine:
- `struct InstallResult { bool ok; std::string error; std::vector<std::string> warnings; };`
- `bool base_layouts_available()` — every target a theme/pack needs has a base
  `.szs` present in `base_layout_dir()`. (For the generic case: at least the
  qlaunch base set exists.)
- `InstallResult install(const core::ThemeDetail& detail)` — for each part:
  read the `.nxtheme` from its downloaded path, read the matching base `.szs`
  from `base_layout_dir()`, run the engine's patcher (background BNTX + BFLYT/
  BFLAN layout patches), write the output `.szs` to `output_szs_path(target)`,
  and create marker files (`fsmitm.flag` per title dir, `version_hash.bin` under
  `0100000000001000`). Aggregate engine compatibility warnings. On any hard
  failure, roll back files written in this call.
  On success, record state via `active_theme_store`.
- `InstallResult remove_active()` — delete the `.szs` outputs (and now-empty
  `lyt/` dirs) recorded as active, then clear `active_theme_store`.

#### 4. `source/platform/themes/active_theme_store.{hpp,cpp}` (`namespace thomaz`)
Persists `/themes/.thomaz_active.json`. Testable with a temp dir:
- `struct ActiveTheme { std::string hex_id; std::string name; std::string author; std::vector<std::string> targets; };`
- `std::optional<ActiveTheme> get_active();`
- `void set_active(const ActiveTheme&);`
- `void clear();`
- `bool is_active(const core::ThemeEntry&);` — `hex_id` match, for the badge.

#### 5. `source/platform/system/reboot.{hpp,cpp}` (`namespace thomaz`)
- `void reboot_to_payload();` — Switch: `spsmInitialize(); spsmShutdown(true);`
  (matches upstream `PlatformReboot`). Desktop: no-op (log only).

#### 6. UI — `theme_detail_activity` + `theme_browser_activity`
- **Button state machine** in detail (`detailDesc`-side download button):
  - not downloaded → `themes/download` (existing) → on success re-resolve state.
  - downloaded & not active → `themes/apply` (pack → `themes/apply_pack`).
  - active (matches this entry) → `themes/remove`.
- **Apply flow:** if `!base_layouts_available()` → dialog `themes/base_missing`
  (with the how-to and a Cancel). Else show `themes/applying`, run
  `theme_install::install` on a worker (`brls::async`), then on the UI thread:
  notify `apply_ok` (+ `warn_parts_removed` if warnings), refresh button to
  **Remover**, and open the reboot dialog.
- **Remove flow:** `themes/removing` → `remove_active()` → `remove_ok` → reboot
  dialog → button back to **Aplicar**.
- **Reboot dialog:** `themes/reboot_prompt` with `themes/reboot_now`
  (→ `reboot_to_payload()`) and `themes/reboot_later`.
- **Applied badge:** browser grid card and detail header show `[Aplicado]` when
  `active_theme_store::is_active(entry)`.

#### 7. i18n — `resources/i18n/{en-US,pt-BR}/themes.json`
New keys: `apply`, `apply_pack`, `applying`, `apply_ok`, `apply_fail`,
`remove`, `removing`, `remove_ok`, `remove_fail`, `base_missing`,
`base_missing_help`, `reboot_prompt`, `reboot_now`, `reboot_later`, `applied`,
`warn_parts_removed`.

## Data flow (apply)

```
tap Aplicar
  → base_layouts_available()?  ── no → dialog base_missing (stop)
  → yes → brls::async:
       for each part in detail:
         read sd:/themes/<folder>/<part>.nxtheme
         read base_layout_dir()/<target szs>
         engine: SzsPatcher(base).PatchBgLayouts/PatchBntx(nxtheme)  → patched szs (+warnings)
         write layeredfs_root()/<titleid>/romfs/lyt/<target szs>
         ensure <titleid>/fsmitm.flag, 0100000000001000/version_hash.bin
       active_theme_store.set_active({hex_id,name,author,targets})
  → brls::sync: notify apply_ok (+warn_parts_removed), button→Remover, reboot dialog
```

## Error handling

- **Missing base layouts** → `base_missing` dialog with how-to; no write.
- **Corrupt / unsupported `.nxtheme`** (engine throws / too-new manifest) →
  `apply_fail` with the engine message; nothing left half-written (rollback).
- **Per-part compatibility drops** (engine `LayoutCompatibility`) → not a
  failure; collected into `warnings` and shown via `warn_parts_removed`.
- **Pack partial failure** → treat the whole apply as failed and roll back the
  files written in this call, so the console isn't left with a half-applied pack.
- **CFW root not found** (desktop / unknown setup) → `apply_fail` explaining no
  LayeredFS target.
- All file IO failures surface a human message; never crash the UI thread
  (engine work runs in `brls::async`, results marshalled back via `brls::sync`,
  guarded by the activity `alive` flag like the existing theme screens).

## Testing

- **Unit (desktop, `tests/Makefile`):**
  - `active_theme_store` — set/get/clear/is_active round-trip in a temp dir.
  - `cfw_paths` — target→title/szs mapping; `base_szs_path`/`output_szs_path`.
  - `theme_install::base_layouts_available()` — temp dir with/without base files.
  - These pure/IO units are added to the hand-globbed test `SRCS` (the engine and
    `theme_install::install` itself are excluded from the unit build — they need
    real SZS fixtures and the engine link).
- **Engine:** trust the upstream port; optional desktop **smoke** that patches a
  bundled sample base `.szs` with a sample `.nxtheme` and asserts a non-empty
  SARC output. Real validation is on hardware.
- **Hardware:** apply a theme, apply a pack, remove, reboot, verify the badge
  and that the home menu shows/clears the theme.

## Build

- `GLOB_RECURSE` already picks up new `source/` files; add `lib/switchthemes/`
  to the target's include + sources. No new system dependencies (Yaz0 is
  self-contained; `miniz` and `stb_image` are vendored single-files; `nlohmann/
  json` already present).
- Engine compiles for both Switch and desktop; `reboot` and the LayeredFS root
  degrade to no-op / local dir on desktop so the boot smoke still passes.
- `tests/Makefile` `SRCS` gains only the three new pure/IO units (not the engine).

## Licensing / legal

- Replace `LICENSE` (MIT) with **GPLv2**.
- Add `NOTICE` / `THIRD_PARTY.md` crediting `exelix11/SwitchThemeInjector`
  (GPLv2) and the vendored single-headers (`stb_image`, `miniz`) with their
  licenses; record the upstream commit in `lib/switchthemes/README`.
- Update the README license section (currently "Distribuído sob a licença MIT").

## Risks / open questions

- **Base-layout coverage:** a theme may target a title whose base `.szs` the user
  hasn't dumped. `base_layouts_available()` must check the specific targets the
  selected theme/pack needs, and the `base_missing` message must name them.
- **Firmware drift:** the engine drops incompatible parts; a theme built for a
  very different firmware may apply with several parts removed. The
  `warn_parts_removed` message must make that visible so the result isn't
  silently partial.
- **Desktop engine compile:** the upstream C++ assumes some libnx-free but
  Switch-leaning constructs; the vendoring task must keep it building on g++ for
  the unit/smoke build (or `#ifdef`-guard the few Switch-only spots).
