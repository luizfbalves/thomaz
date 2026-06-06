# Stack Research — v1.2 Game Management (Tinfoil-style content client)

**Domain:** On-device Nintendo Switch title install/uninstall/list (base + update + DLC), consuming a Tinfoil-style HTTP JSON content index
**Researched:** 2026-06-06
**Confidence:** HIGH for libnx services and existing-infra reuse (verified against current libnx + Awoo/Tinfoil source); MEDIUM-HIGH for NCZ/NSZ details (verified against the nsz format spec + Awoo nca_writer).

---

## TL;DR for the roadmapper

**Almost nothing new needs to be added to the toolchain.** The install/uninstall/list pillar is built on libnx services that ship in the toolchain thomaz already uses (`ncm`, `ns`, `es`, `fs`, `set`/`spl`), plus zstd which is **already on the link line** (libarchive pulls it). The genuinely net-new work is *code* (an NSP/PFS0 container parser and an NCM placeholder-install state machine), not *dependencies*.

| Capability | What provides it | New dependency? |
|------------|------------------|-----------------|
| List installed titles + versions | `ns` (`nsListApplicationRecord`, `nsListApplicationContentMetaStatus`) — **already implemented** in `title_service_switch.cpp` | No |
| Free/total space NAND + SD | `ns` (`nsGetTotalSpaceSize`/`nsGetFreeSpaceSize`) — **already implemented** in `system_status.cpp` | No |
| NCA write to storage | `ncm` (`ncmContentStorageCreatePlaceHolder` / `WritePlaceHolder` / `Register`) | No (libnx) |
| Content-meta registration (CNMT) | `ncm` (`ncmContentMetaDatabaseSet` / `Commit`) | No (libnx) |
| Make a title appear / update / DLC visible | `ns` (`nsPushApplicationRecord`) | No (libnx) |
| Ticket / cert import (for titlekey crypto titles) | `es` (`esImportTicket`) | No (libnx) |
| Uninstall | `ns` (`nsDeleteApplicationCompletely`) + `ncm` deletes | No (libnx) |
| Download from linked server | **existing** `http_client_curl` | No |
| Local SD/USB read | stdio / `fs` mount (USB via `usbHsFs` only if USB-HDD support is wanted — see "What NOT to add") | No (SD); optional lib for USB |
| NSP/PFS0 container parse | **net-new C++ code** (no lib — it's a trivial header format) | No (code only) |
| NSZ/NCZ zstd decompress + re-encrypt | **net-new C++ code** over the **already-linked** `libzstd` | No (zstd already linked) |

**Do NOT add:** `prod.keys`-dependent NCA decryption for plain NSP install (install does not decrypt — it copies ciphertext); a separate zstd portlib; hactool for install; a full XCI/gamecard path unless XCI install is in scope.

---

## Recommended Stack

### Core Technologies (libnx services — already in the toolchain)

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| libnx `ncm` (Content Manager) | libnx 4.11.1 (current; `v4.11.1`, 2025-02-04) | NCA content storage: create/write/register placeholders; content-meta DB set/commit; per-storage free space | The only sanctioned on-device path to write NCAs into a storage (BuiltInUser/SdCard) and register their content-meta. Every homebrew installer (Tinfoil, Awoo, GoldLeaf) uses exactly these calls. |
| libnx `ns` (NS Application Manager) | libnx 4.11.1 | Push application records (`nsPushApplicationRecord`) so a freshly-registered title/update/DLC becomes visible to the home menu; list installed records + content-meta status; total/free space; delete. | thomaz **already** uses `ns` for listing and space. The same service finalizes an install (record push) and performs uninstall. No new init surface. |
| libnx `es` (e-shop/ticket) | libnx 4.11.1 | Import the title's ticket + cert (`esImportTicket`) for titlekey-crypto titles | NSPs that ship a `.tik`/`.cert` carry a titlekey-encrypted titlekey; without `esImportTicket` the title installs but won't launch. Required for correctness; cheap to add (init `es`, one import call). |
| libnx `fs` (Filesystem) | libnx 4.11.1 | Open `NcmContentStorage` targets, query storage, mount SD; (already used for BIS in theme key loader) | Already initialized/used across thomaz. Reused, not new. |
| libnx `spl` (already linked) | libnx 4.11.1 | **Only if** verifying/decrypting is ever needed — NOT needed for plain NSP install. Listed for completeness because theme code already uses it. | Plain NSP install copies encrypted NCAs as-is; no SPL key derivation needed. (Contrast with NCZ — see note below.) |
| zstd (`libzstd`) | the version in devkitPro `switch-zstd` portlib (pulled transitively by `switch-libarchive`) — **already on the link line** | Streaming decompression of NCZ-compressed NCA bodies for NSZ install | **Already linked** (`CMakeLists.txt` link line: `archive bz2 lzma zstd lz4 z`). NSZ = NSP whose NCAs are NCZ (zstd-compressed). Reusing the linked zstd avoids any new dependency; only the `<zstd.h>` include + `ZSTD_decompressStream` usage is new code. |

### Supporting Libraries / Code Modules

| Library / Module | Version | Purpose | When to Use |
|------------------|---------|---------|-------------|
| **PFS0/NSP parser (net-new code)** | n/a — write it | Parse the NSP container: PFS0 magic, file-count, string-table, per-file (offset,size,name) entries; locate the CNMT NCA, program/control NCAs, `.tik`/`.cert` | Required for every NSP/NSZ install (remote and local). The format is a fixed 16-byte header + tables — a few dozen lines of C++; no library exists or is warranted. |
| **NCZ section decoder (net-new code)** | n/a — write it | Read the NCZ header at offset `0x4000` (section list with per-section `offset/size/cryptoType/cryptoKey/cryptoCounter`, optional block header), `ZSTD`-decompress the body to `0x4000`, AES-CTR **re-encrypt** each section using the keys carried *in the NCZ header*, write the reconstructed NCA via the NCM placeholder | Required only for NSZ/XCZ (compressed) sources. **Key fact:** the per-section AES-CTR keys live inside the NCZ header — re-encryption does **not** need `prod.keys` or SPL. This is independent of thomaz's existing SPL/hactool theme-extraction path. |
| **nlohmann/json (vendored, header-only)** | already vendored at `lib/json` | Parse the Tinfoil-style content index (`{"files":[{"url":..,"size":..},...],"directories":[...],"success":...}` shape) and any per-title metadata | Reuse — already the project's JSON lib. No new dep. |
| **AES-CTR primitive** | from the **already-vendored** Mbed TLS (CMAC build) or libnx `aes128CtrContext*` | Re-encrypt NCZ-decompressed sections | Prefer libnx's built-in `aes128CtrContextCreate` / `aes128CtrCrypt` (in `<switch/crypto/aes_ctr.h>`) — zero new code/links — over re-using the isolated hactool mbedtls. Only needed for the NSZ path. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| `nsz` (Python, nicoboss/nsz) | Local repro: compress/decompress NSZ↔NSP to generate test fixtures and validate the on-device NCZ decoder against a reference | Dev-only, off-device. Use it to produce known-good NSP and matching NSZ for the same title to test the decoder. |
| Awoo-Installer / Tinfoil source | Reference implementation of the exact libnx call sequence (install state machine, ticket import, NCZ writer) | GPL-licensed reference — read for the algorithm; do not vendor wholesale (license + scope). |

---

## The NSP install sequence (concrete libnx calls, in order)

Verified against Awoo-Installer `source/install/install.cpp` + `install_nsp.cpp` + `nca_writer.cpp` and libnx `ncm.h` (current). This is the algorithm the roadmap/requirements should encode:

1. **Parse the NSP (PFS0).** Read the PFS0 header → enumerate entries → find the CNMT NCA (`*.cnmt.nca`), the `.tik`/`.cert` (if present), and the program/control/data NCAs. *(net-new parser)*
2. **Read the CNMT.** The CNMT NCA's meta section yields the `NcmContentMetaKey` (title id, version, meta type: Application / Patch / AddOnContent) and the list of `NcmContentInfo` (content id + size + type per NCA). *(needs reading the CNMT — for plain NSP this is a structured read; thomaz's existing NCA/RomFS reader is theme-specific, so expect a small CNMT reader here.)*
3. **Open the target content storage:** `ncmOpenContentStorage(&cs, storageId)` where `storageId` is `NcmStorageId_SdCard` or `NcmStorageId_BuiltInUser` (choose per user/free-space). Query headroom with `ncmContentStorageGetFreeSpaceSize`.
4. **For each NCA** (and the CNMT NCA):
   - `ncmContentStorageCreatePlaceHolder(&cs, &contentId, &placeholderId, size)`
   - loop: `ncmContentStorageWritePlaceHolder(&cs, &placeholderId, offset, buf, n)` while streaming bytes from the download (or NCZ decoder) — **this is where the resumable queue feeds data and where `http_client_curl` is reused**
   - `ncmContentStorageRegister(&cs, &contentId, &placeholderId)`
   - on failure/cancel: `ncmContentStorageDeletePlaceHolder` to avoid orphaned placeholders.
5. **Register content meta:** `ncmOpenContentMetaDatabase(&db, storageId)` → build a `NcmContentMetaHeader` + content/meta-info records from the CNMT → `ncmContentMetaDatabaseSet(&db, &metaKey, buf, size)` → `ncmContentMetaDatabaseCommit(&db)`.
6. **Import ticket/cert (titlekey titles only):** `esImportTicket(tikBuf, tikSize, certBuf, certSize)` (init `es` first). Skip when the title uses standard-crypto only / no `.tik`.
7. **Push the application record:** build a `ContentStorageRecord[]` (each = `{NcmContentMetaKey, storageId}`) for the base title (and any existing meta if extending), then `nsPushApplicationRecord(baseTitleId, NsApplicationRecordType_Installed, records, recordCount)`. For updates/DLC this makes the home menu surface the patch/add-on. (Some flows first `nsDeleteApplicationRecord` / `nsCountApplicationContentMeta` + `nsListApplicationRecordContentMeta` to merge with existing records — relevant for "install DLC onto an already-installed base".)

**Uninstall:** for a whole title `nsDeleteApplicationCompletely(titleId)`. For granular update/DLC removal, delete the relevant content + meta via `ncm` and re-push a trimmed application record. (Granular DLC/update uninstall is materially trickier than full-title delete — flag for phase-level research.)

**Update vs DLC vs base** are the *same* install pipeline differing only in the CNMT meta type (`Patch` / `AddOnContent` / `Application`) and in how the application record is merged — not in the libnx calls used.

---

## Reuse map — existing thomaz infra vs net-new

| Existing infra | Reusable for v1.2? | Notes |
|----------------|--------------------|-------|
| `http_client_curl` (TLS policy, cancellation, abort-on-teardown) | **Yes — directly.** | Drives downloads from the linked content server; its existing cancellation/abort wiring is exactly what the resumable queue needs. Add HTTP `Range` request support if not already present (for resume). |
| `ns` usage in `title_service_switch.cpp` / `system_status.cpp` | **Yes — extend.** | Listing + space already done. Add record-push + delete on the same service. `ns` is already init'd at startup. |
| Borealis game list with icons/titledb | **Yes.** | Cover-art browse + installed-title list reuse this UI substrate. |
| libarchive extractor (`libarchive_extractor.cpp`) | **No — wrong tool.** | NSP is **not** a general archive (it's PFS0, no compression); libarchive won't parse it. NSZ uses raw zstd streams inside NCZ, not a zstd-filtered archive container. Use the net-new PFS0 parser + direct `libzstd`. (But the zstd *library* libarchive pulls in is reused — see below.) |
| hactool fork + SPL key derivation (`nca_extract_switch.cpp`, `key_loader_switch.cpp`) | **Mostly no.** | That stack *decrypts* NCAs (theme RomFS extraction) using on-device SPL-derived keys. **Install does the opposite** — it writes already-encrypted NCAs verbatim; no decryption, no `prod.keys`, no SPL. The CNMT read needed for install is a lighter parse than hactool's full pipeline. SPL/hactool are **not** required for plain NSP and **not** required for NSZ (NCZ carries its own section keys). |
| Vendored Mbed TLS (CMAC, isolated) | **Optional, secondary.** | Could supply AES-CTR for NCZ re-encryption, but prefer libnx's built-in `aes128Ctr*` (no extra link, no symbol-isolation concerns). |
| zstd on the link line (`archive bz2 lzma zstd lz4 z`) | **Yes — reuse the link, add the include.** | NSZ decompression links the *same* `libzstd`; only `#include <zstd.h>` + `ZSTD_createDStream`/`ZSTD_decompressStream` calls are new. No CMake link-line change needed. |
| nlohmann/json (`lib/json`) | **Yes.** | Parse the Tinfoil JSON index and metadata. |
| Cloud API (Fastify + Prisma + Postgres, JWT) | **Yes — extend schema.** | "One-tap server linking synced to the account" = store the user's content-server URL + optional auth on the account (new Prisma model/route). No new client dependency. |

---

## Consuming the Tinfoil-style HTTP JSON index

- **Format:** Tinfoil "shop" index is a JSON document, canonically `{"files":[{"url":"<abs-or-rel url>","size":<bytes>}, ...], "directories":[...], "success":"<message>", "referrer":..., "version":...}`. Some servers gzip the body and/or AES-encrypt it (Tinfoil's encrypted-index scheme); for an MVP target **plain/gzip JSON** and treat the encrypted-index variant as out of scope unless required.
- **Transport:** reuse `http_client_curl`. Optional auth = HTTP Basic / custom header / query token — all expressible through the existing client. Honor `Range` for resume.
- **Parsing:** nlohmann/json. Map each entry to a download job; filename conventions (`[titleid][version].nsp`/`.nsz`) drive base/update/DLC classification and "auto-detect updates/DLC" by cross-referencing `nsListApplicationRecord` output.
- **Confidence:** MEDIUM on the exact index schema variants (Tinfoil's format is community-reverse-engineered, not formally specified). Recommend a phase-level spike that points the parser at a real user server early.

---

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| Direct libnx `ncm`/`ns`/`es` install state machine | Vendoring Awoo/Tinfoil install module wholesale | Never wholesale (GPL + scope creep). Read it as the reference algorithm; implement thomaz's own thin state machine in `platform/`. |
| Built-in libnx `aes128Ctr*` for NCZ re-encrypt | Reuse the isolated hactool mbedtls AES | Only if a libnx AES-CTR limitation surfaces on hardware; otherwise avoid touching the symbol-isolated mbedtls. |
| Reuse the already-linked `libzstd` | Add `switch-zstd` as an explicit portlib | Unnecessary — it's already pulled in transitively and on the link line. Adding it explicitly is harmless but redundant. |
| NSP + NSZ support | NSP only (defer NSZ) | Valid MVP cut: ship plain NSP install first (no zstd/NCZ code), add NSZ in a later phase. NSZ is the higher-complexity, higher-risk piece. |
| SD + NAND install targets | SD-only first | Reasonable MVP simplification; NAND (`BuiltInUser`) install adds free-space/edge-case handling. |

---

## What NOT to Use / Add

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| hactool / SPL key derivation for **install** | Install writes encrypted NCAs verbatim — it never decrypts. Wiring SPL/hactool into the install path adds the `prod.keys`-class complexity the theme feature fought, for zero benefit. | Plain placeholder writes via `ncm`. Keep SPL/hactool scoped to theme extraction. |
| `prod.keys` dependency | Neither plain NSP nor NSZ install needs the console master keys: NSP copies ciphertext; NCZ carries its own section keys in its header. | No keys for install. (Ticket import via `es` handles titlekey titles.) |
| libarchive for NSP/NSZ | NSP is PFS0 (uncompressed container), not zip/7z/tar; NSZ is a raw zstd stream inside NCZ, not an archive. libarchive will not parse either. | Net-new PFS0 parser + direct `libzstd`. |
| A separate/duplicate zstd portlib | `libzstd` is already linked transitively via `switch-libarchive`. | Reuse the existing link; just add `<zstd.h>`. |
| `usbHsFs` USB-HDD mounting (for now) | Adds a substantial dependency + UX surface (mounting external USB filesystems) for a feature ("install from USB") that the milestone lists loosely. SD + linked-server cover the core value. | SD/local file read via stdio; defer USB-HDD to a later phase if demand is real. Flag as a scope decision. |
| Full XCI/XCZ (gamecard image) install path | XCI adds HFS0 partition parsing + gamecard-specific handling on top of NSP. Not required for the "content server + SD" core value. | Scope to NSP/NSZ first; treat XCI/XCZ as a future add. |
| Tinfoil **encrypted** index support in MVP | The AES-encrypted index variant is extra crypto + key handling for a minority of servers. | Support plain/gzip JSON index first; revisit encrypted index if a target server needs it. |

---

## Stack Patterns by Variant

**If MVP scope = "remote NSP install from linked server + list/uninstall" (recommended first cut):**
- libnx `ncm` + `ns` + `es` + existing `http_client_curl` + net-new PFS0/CNMT parser + nlohmann/json.
- **No** zstd/NCZ code, **no** SPL/hactool, **no** new toolchain deps at all.
- Lowest-risk path to a shippable pillar.

**If NSZ (compressed) support is in scope:**
- Add the NCZ section decoder over the already-linked `libzstd` + libnx `aes128Ctr*`.
- Highest-complexity sub-feature; recommend its own phase with an on-hardware validation gate (NCZ re-encryption bugs produce silently-corrupt installs that only fail at game launch).

**If granular update/DLC uninstall is required (vs full-title delete):**
- Needs application-record merge/trim logic (`nsListApplicationRecordContentMeta` + re-`nsPushApplicationRecord`) beyond `nsDeleteApplicationCompletely`. Flag for phase-level research.

---

## Version Compatibility

| Component | Compatible With | Notes |
|-----------|-----------------|-------|
| libnx 4.11.1 (current `v4.11.1`, 2025-02-04) | the `ncm`/`ns`/`es` calls above | All listed functions exist in current libnx; confirmed via switchbrew libnx docs. No version bump required beyond the toolchain thomaz already ships. |
| `ncm`/`ns` content-meta structs | firmware-version-sensitive | NCM content-meta record layout has historically grown across firmware; installers track this. Test on the actual target firmware; do not hardcode struct sizes where libnx provides them. Flag as a hardware-validation item. |
| `libzstd` (portlib) | NCZ streams produced by `nsz` | NCZ uses standard zstd frames; any modern `libzstd` decodes them. The linked version is adequate. |
| `es` service | titlekey-crypto titles | `esImportTicket` is the standard path; behavior stable across recent firmware. |

---

## Sources

- libnx `ncm.h` reference (switchbrew.github.io/libnx) — confirmed `ncmOpenContentStorage`, `ncmContentStorageCreatePlaceHolder`, `ncmContentStorageWritePlaceHolder`, `ncmContentStorageRegister`, `ncmContentStorageDeletePlaceHolder`, `ncmContentStorageGetFreeSpaceSize`, `ncmOpenContentMetaDatabase`, `ncmContentMetaDatabaseSet`, `ncmContentMetaDatabaseCommit` exist in current libnx — **HIGH**.
- libnx releases (github.com/switchbrew/libnx/releases) — current release `v4.11.1`, 2025-02-04 — **HIGH**.
- Awoo-Installer `source/install/install.cpp`, `install_nsp.cpp`, `nca_writer.cpp` (github.com/Huntereb/Awoo-Installer) + DeepWiki technical notes — install sequence (placeholder write → register → meta set/commit → ns push record → es ticket import) and NCZ writer using ZSTD + AES-CTR re-encrypt — **HIGH** for sequence, **MEDIUM-HIGH** for NCZ details.
- Tinfoil `source/install/install_nsp_remote.cpp` (github.com/shchmue/Tinfoil) — remote NSP install reference — **HIGH**.
- nsz format description (github.com/nicoboss/nsz, NCZ spec) — NCZ header at `0x4000`, per-section crypto keys carried in-file, zstd body from `0x4000` to EOF, optional block compression — **HIGH** for format, confirms re-encryption needs no external keys.
- thomaz codebase: `source/platform/title_service_switch.cpp`, `system_status.cpp`, `themes/key_loader_switch.cpp`, `themes/nca_extract_switch.cpp`, `mods/libarchive_extractor.cpp`, `CMakeLists.txt` (link line `curl mbedtls mbedx509 mbedcrypto archive bz2 lzma zstd lz4 z webp`), `.planning/codebase/STACK.md`, `.planning/research/EXTRACTION.md` — existing-infra reuse map — **HIGH** (direct source read).

---
*Stack research for: Switch on-device title management (install/update/DLC/uninstall/list) + Tinfoil-style HTTP JSON index*
*Researched: 2026-06-06*
