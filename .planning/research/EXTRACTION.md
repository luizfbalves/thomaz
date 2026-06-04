# Research — Native Firmware Layout Extraction (Option A)

**Domain:** Keyless-to-user extraction of Switch home-menu base layouts on-device
**Source:** exelix11/SwitchThemeInjector @ `2618b0c31e007d019757dc4095eca08b4a89e3f5` (GPLv2)
**Researched:** 2026-06-04
**Confidence:** HIGH — verified against the actual `SwitchThemesNX` source, not memory.
**Decision locked:** **Option A** — port the BIS+SPL+hactool mechanism faithfully.

---

## Key correction

The NXThemes console-extraction path is **NOT** `fsOpenFileSystemWithId` / a keyless
FS-service mount. Grep confirms `fsOpenFileSystemWithId`, `ncmContent*`,
`romfsMountFromStorage`, `romfsMountFromFsdev` do **not** appear anywhere in the repo.

What it actually does (`SwitchThemesNX/source/SwitchTools/key_loader.cpp`, `__SWITCH__` branch):

1. **Mount raw BIS System partition** — `fsOpenBisFileSystem(FsBisPartitionId_System, "")`
   + `fsdevMountDevice("System", sys)`. Gives access to the encrypted NCA files on disk.
2. **Resolve title ID → NCA path** via Location Resolver:
   `lrInitialize` → `lrOpenLocationResolver(NcmStorageId_BuiltInSystem)` →
   `lrLrResolveProgramPath(&res, id, path)` → rewrite `@SystemContent://…` to
   `System:/Contents/…nca`.
3. **Derive NCA header key on-device via SPL** (no prod.keys):
   `splCryptoGenerateAesKek(header_kek_source,0,0,tempkek)` →
   `splCryptoGenerateAesKey(tempkek, header_key_source[..], header_key[..])`.
   The `*_source` arrays are PUBLIC key sources (copied from Atmosphère's
   `fssrv_nca_crypto_configuration.cpp`). SPL turns public sources into the real
   per-console key using hardware key slots.
4. **Decrypt + extract RomFS with a bundled hactool fork** (`hactool.cpp`):
   `nca_process` with `ACTION_INFO|EXTRACT|MEMORYONLY`, `extraction_romfs=true`,
   `romfs_filter=FileFilterFunction` (only the requested `/lyt/*.szs`). Files captured
   in-memory via `OnFileDumped`.

So: **keyless to the user, NOT key-free.** It bundles hactool + a custom
`libmbedtls.a` (built with `MBEDTLS_CMAC_C`) — exactly the
`hactool`/`mbedtls`/`key_loader.cpp`/`fs.cpp` glue we deliberately removed in Phase B.
Option A means re-adding it.

---

## Reusable facts (title IDs, szs list, firmware)

`Common.cpp` / `Common.hpp`:

| Title | ID | `.szs` extracted |
|-------|----|------------------|
| qlaunch | `0100000000001000` | ResidentMenu, Entrance, Flaunch, Set, Notification, **common** |
| player select | `0100000000001007` | Psl |
| my page | `0100000000001013` | MyPage |

- Extraction always pulls the same szs names regardless of firmware
  (`GetTargetsForTitleId` only consults `ThemeTargetList6` for names + always adds
  `common.szs` for qlaunch).
- Firmware detection: `setsysGetFirmwareVersion()` → `{major,minor,micro}` + version_hash.
- Compat bucketing enum: `Fw5_0, Fw6_0, Fw8_0, Fw9_0, Fw11_0, Fw20_0` — only matters
  for *patch application*, not which files to dump. Known gotcha: pre-6.0 used
  `common.szs` instead of `ResidentMenu.szs` semantics.

Entry point UI: `NcaDumpPage.cpp` — page "Extract home menu", button
"Extract szs layout files" → `RomfsCache::GetContent(titleId)` → `WriteExtracted` to
`themes/systemData/extracted/{qlaunch,playerselect,mypage}/`.

---

## #1 constraint — TITLE TAKEOVER required

The privileged services (raw BIS, SPL/splCrypto, pmdmnt, lr) **fail when run as an
applet** (hbmenu/album). They only work via **title takeover** (launched as an
Application / forwarder, inheriting broad FS permissions). Repo evidence:
- `PlatformSwitch.cpp`: `appletGetAppletType() != Application` → low-mem applet mode.
- Multiple pages: "not available in applet mode, launch with title takeover."

**Implication for thomaz:** today it runs as a normal NRO (applet). The extraction
feature must run under title takeover, or it will fail at `fsOpenBisFileSystem`.
This is a UX/distribution change, not just code.

---

## License

GPL-2.0 for the whole repo incl. `SwitchThemesNX` (root `/LICENSE`, no separate NX
license). thomaz is already GPLv2 → porting `key_loader.cpp` + `hactool.cpp` +
`RomfsCache.cpp` + the hactool fork + mbedtls is legally fine. Extracted Nintendo
`.szs` remain copyrighted — never distribute them.

---

## Minimal port plan (Option A)

Files/functions to reimplement in our libnx app:

1. **`key_loader.cpp` (`__SWITCH__` branch)** — `ExtractionContext`:
   ctor (`pmdmntInitialize`+`splInitialize`+`splCryptoInitialize`+`fsOpenBisFileSystem(System)`+`fsdevMountDevice`),
   `getNcaPath(titleId)` (lr resolve + path rewrite), `Initialize()` (SPL key derivation),
   `LoadKeys()` (copy derived header key + KAK app source into hactool keyset).
2. **`hactool.cpp`** — `HactoolHelper::Process()` + `ExtractFiles()` (in-memory NCA RomFS
   extraction with filename filter + dump callback).
3. **`RomfsCache.cpp`** (optional cache) + the `ThemeTargetInfo` title-ID/szs tables from
   `Common.{hpp,cpp}`.
4. **Re-vendor dependencies removed in Phase B:** the hactool fork (`Libs/hactool`) and
   the custom `libmbedtls.a` (`MBEDTLS_CMAC_C`), per Makefile `LIBS`/`LIBDIRS`.
5. **Run as title takeover (Application), not applet** — otherwise BIS/SPL/pmdmnt/lr fail.
6. Wire output to our canonical `/themes/systemData/` (note: exelix writes to
   `themes/systemData/extracted/{qlaunch,...}/` — confirm our `cfw_paths::base_layout_dir()`
   flat layout vs their subdir layout; adapt one to the other).

## Open questions — HARDWARE ONLY

1. Do the pinned public key **sources still derive a valid header key on the target
   firmware** (newer master keygens may need updated sources)? — **highest risk.**
2. `lrLrResolveProgramPath` signature/availability across FW (5.x vs 11.x vs 20.x).
3. Exact set of `/lyt/*.szs` present per firmware (does `common.szs` exist; pre-6.0 semantics).
4. Title-takeover launch path for thomaz (forwarder NSP vs hbloader title override) and
   the resulting permission set.

## Permalinks (pinned to 2618b0c)
- key_loader.cpp: https://github.com/exelix11/SwitchThemeInjector/blob/2618b0c31e007d019757dc4095eca08b4a89e3f5/SwitchThemesNX/source/SwitchTools/key_loader.cpp
- hactool.cpp: https://github.com/exelix11/SwitchThemeInjector/blob/2618b0c31e007d019757dc4095eca08b4a89e3f5/SwitchThemesNX/source/SwitchTools/hactool.cpp
- RomfsCache.cpp: https://github.com/exelix11/SwitchThemeInjector/blob/2618b0c31e007d019757dc4095eca08b4a89e3f5/SwitchThemesNX/source/SwitchTools/RomfsCache.cpp
- NcaDumpPage.cpp: https://github.com/exelix11/SwitchThemeInjector/blob/2618b0c31e007d019757dc4095eca08b4a89e3f5/SwitchThemesNX/source/Pages/NcaDumpPage.cpp
- Common.cpp: https://github.com/exelix11/SwitchThemeInjector/blob/2618b0c31e007d019757dc4095eca08b4a89e3f5/SwitchThemesNX/source/SwitchThemesCommon/Common.cpp
- LICENSE: https://github.com/exelix11/SwitchThemeInjector/blob/2618b0c31e007d019757dc4095eca08b4a89e3f5/LICENSE
