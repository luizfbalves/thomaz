# Feature Research

**Domain:** Nintendo Switch homebrew game-management client (Tinfoil-style content installer) — milestone v1.2 of thomaz
**Researched:** 2026-06-06
**Confidence:** HIGH (Tinfoil index format + installer behaviors from official docs and primary sources; HIGH on existing-infra fit because the relevant libnx APIs and zstd-capable libarchive are already linked and in use in this tree)

---

## Scope framing (read first)

thomaz ships **only the client**. The user supplies their own content server (an HTTP/HTTPS JSON index) and/or local SD/USB files. thomaz hosts and distributes nothing. Every feature below is a *client capability over user-supplied sources* — the same legal posture as a generic HTTP download manager + the device's own install API. Nothing here should bundle, link to, or default-configure any content source. See **Policy-sensitive flags** at the end.

---

## How these clients work (user's mental model)

The reference clients (Tinfoil, Awoo Installer, DBI, Goldleaf) converge on one flow that users already expect:

1. **Add a source.** Either point at a remote "shop" (a URL serving a JSON index) or browse local SD/USB. Remote sources may require auth (HTTP basic auth in the URL, a custom header, or a referrer/MotD gate).
2. **Browse a catalog.** The client fetches the index, shows entries grouped as **New Games / New Updates / New DLC**, ideally with cover art + metadata (from a titledb).
3. **Pick + install.** Select an entry; the client downloads the package (NSP/NSZ/XCI/XCZ), verifies it, and installs via the device's content-management APIs. NSZ/XCZ are zstd-compressed and must be decompressed during install.
4. **Manage what's installed.** List installed base titles with version, attached updates, DLC count, and on-disk size; show free space on NAND and SD; uninstall base/update/DLC.

thomaz's differentiation is purely UX: cover-art-rich browse, one-tap server linking synced to the cloud account, a resumable queue with progress/cancel, and auto-detected updates/DLC — none of which the stock installers do well (Tinfoil/Awoo are list-first and queue-thin; DBI is powerful but utilitarian; Goldleaf is a file manager, not a shop).

---

## The Tinfoil "shop" index format (concrete, public convention)

A shop is a JSON document served over HTTP(S). Tinfoil-compatible servers (e.g. TinfoilWebServer, NUT) emit it; clients parse it. Supporting *reading* this format is what makes thomaz interoperate with the existing ecosystem of user-run servers. **Confidence: HIGH** (blawar.github.io/tinfoil official docs + TinfoilWebServer README).

### Top-level fields

| Field | Type | Meaning | thomaz needs to read? |
|-------|------|---------|------------------------|
| `files` | array of `{url, size}` or string | The catalog. Each entry is a downloadable package URL + byte size. | **Yes (core)** |
| `directories` | array of URL strings | Sub-indexes to recurse into. | Yes (v1 can flatten/ignore-or-fetch) |
| `success` | string | "Message of the day" shown on connect. | Yes (display) |
| `error` | string | Error MotD. | Yes (display) |
| `referrer` | string | Anti-hotlink: client must send this as `Referer`. | **Yes (auth)** |
| `headers` | array of `"Key: Value"` | Extra HTTP headers to send on file requests (e.g. `Authorization: Bearer …`). | **Yes (auth)** |
| `version` | number | Minimum client version to load the index. | Yes (gate/warn) |
| `titledb` | object keyed by 16-hex title ID | Metadata overrides: `name`, `region`, `version`, `releaseDate`, `rating`, `publisher`, `description`, `size`, `rank`. | Yes (metadata) |
| `googleApiKey` | string | For `gdrive:/` URLs. | **No (out of scope)** |
| `oneFichierKeys` | array | For `1f:/` URLs. | No (out of scope) |
| `clientCertPub` / `clientCertKey` | string | mTLS client cert. | Optional (advanced auth) |
| `themeBlackList` / `themeWhiteList` / `themeError` | array / string | Tinfoil-theme gating. | **No (Tinfoil-specific)** |
| `locations` | array of `{url, title, action}` | User file-browser shortcuts. | No (v1) |

### File-entry URL conventions

- Plain absolute URLs: `https://host/path/Game.nsp`. Plus protocol prefixes thomaz can ignore: `gdrive:/`, `1f:/`, `sdmc:/`.
- A `#filename` shebang can override the displayed/saved name: `https://host/x?id=5#Game [0100…][v0].nsp`.
- **The title ID + type live in the filename, not in JSON.** Convention (from NUT/TinfoilWebServer): the filename must contain `[titleid]` in brackets to be classified, e.g. `Super Mario Odyssey [0100000000010000][v0].nsp`.

### Deriving Game vs Update vs DLC (no explicit field — derived from the title ID)

This is a numeric convention on the 64-bit title ID, **not** a label in the JSON. thomaz must compute it:

- **Base game:** title ID divisible by `0x2000` (the canonical app ID).
- **Update/patch:** `baseId + 0x800`.
- **DLC (add-on content):** `(baseId & ~0xFFF) + 0x1000 + n` (the high bits of the base + `0x1000` + the DLC index).

`[vNNN]` in the filename is the package version (0 for base/first release; updates carry the patch version). Classification into New Games / New Updates / New DLC is done by combining this derivation with the installed-title list (see below).

### Minimal index example

```json
{
  "files": [
    { "url": "https://my.host/Title A [0100AAAA00000000][v0].nsp",  "size": 5242880000 },
    { "url": "https://my.host/Title A [0100AAAA00000800][v131072].nsz", "size": 314572800 },
    { "url": "https://my.host/Title A DLC [0100AAAA00001000][v0].nsp", "size": 104857600 }
  ],
  "directories": ["https://my.host/more/"],
  "success": "Welcome",
  "referrer": "https://my.host/index.tfl",
  "headers": ["Authorization: Bearer abc123"],
  "titledb": {
    "0100AAAA00000000": { "name": "Title A", "publisher": "Studio", "region": "US" }
  }
}
```

### Auth mechanisms a user-server may demand (all client-side, all supported by existing curl client)

1. **HTTP Basic auth in the URL** — `https://user:pass@host/`.
2. **Custom header** — server requires `Authorization:` (or arbitrary) header; supplied via the index `headers` array *or* the user's stored source config.
3. **Referrer gate** — client must echo the `referrer` value as the `Referer` header.
4. **mTLS client cert** — `clientCertPub`/`clientCertKey` (advanced; defer).

---

## Feature Landscape

### Table Stakes (Users Expect These)

Missing any of these makes thomaz feel like a toy next to Awoo/DBI.

| Feature | Why Expected | Complexity | Notes / thomaz dependencies |
|---------|--------------|------------|------------------------------|
| Link a remote source by URL | The whole point of a "shop" client. | LOW | Reuses `http_client_curl` (TLS + cancel already wired). Store URL in a new source config. |
| Parse Tinfoil index JSON (`files`/`directories`/`success`/`titledb`) | Interop with the entire ecosystem of user-run servers. | MEDIUM | New `core/` JSON parser (pure, doctest-covered) like existing `themezer_json`. |
| Source auth: basic-auth URL, custom header, referrer | Most private user servers gate access. | LOW–MEDIUM | curl supports all three; thread through from source config + index `headers`. |
| Browse catalog as a list with name/size | Baseline browse. | LOW | List view; size via existing `storage_format`. |
| Install base title (NSP/XCI) | Core operation. | HIGH | New `platform/` installer using libnx **NCM/NS/ES** APIs (place NCAs, install ticket+cert, push application record). NCA handling already exists in `themes/nca_extract_switch.cpp` — same API family. |
| Install update + DLC | Core operation; users expect parity with base. | MEDIUM (after base install exists) | Same install pipeline; classification by title-ID derivation. |
| Handle NSZ/XCZ (zstd-compressed) | The majority of modern shared packages are NSZ; an NSP-only installer is seen as broken. | HIGH | Must decompress zstd NCAs during install. **zstd is already on the link line** (libarchive built with zstd+lz4 — see `CMakeLists.txt`). Solid-compressed NSZ must be fully decompressed before install. |
| Install from local SD/USB file | Offline installs; not everyone runs a server. | MEDIUM | File browser → same install pipeline. USB-mounted storage adds device-mount handling. |
| List installed titles (base) with version + size | "What do I have?" is half the value. | LOW | **Already built** — `title_service_switch.cpp` uses `nsListApplicationRecord` + `nsGetApplicationControlData` + `nsListApplicationContentMetaStatus`. Reuse directly. |
| Show free space (NAND + SD) | Users must know if an install fits. | LOW | Reuse `system_status` / `storage_format` patterns. |
| Uninstall base / update / DLC | Lifecycle completeness; recover space. | MEDIUM | libnx `ns` delete APIs per content type; must distinguish the three. |
| Per-install progress + error reporting | A multi-GB silent bar is unacceptable. | MEDIUM | Bytes-downloaded/installed callback; reuse curl progress + activity async pattern. |
| NCA header / signature sanity check before install | Brick-avoidance is a known table stake (Awoo warns on modified NSPs). | MEDIUM | Validate NCA magic/signature; surface a warning, do not silently install. |

### Differentiators (Competitive Advantage — the stated Core Value)

These are where thomaz wins. They align directly with PROJECT.md's "smoother, cover-art-rich, resumable flow."

| Feature | Value Proposition | Complexity | Notes / dependencies |
|---------|-------------------|------------|------------------------|
| Cover-art + metadata browse grid | Stock installers are text lists; thomaz already renders game grids. | MEDIUM | Reuse `game_list_activity` / `game_panel` grid + `image_transcode` for cover decode; metadata from index `titledb` and/or existing titledb. |
| One-tap server linking synced to cloud account | Set up once, every signed-in device has the source. No re-typing URLs/headers on each console. | MEDIUM | New API route + table (model on existing `saves` sync + JWT auth + optimistic locking). Store URL + auth blob server-side (encrypted-at-rest consideration). |
| Resumable download/install **queue** with progress + cancel | Multi-GB installs over flaky Wi-Fi; queue several and walk away. The single biggest UX gap in Tinfoil/Awoo. | HIGH | HTTP range requests for resume (curl supports `Range`); persist queue state to survive app exit; per-item cancel reuses the curl-abort-on-teardown mechanism. Sequential install (parallel installs to NCM are unsafe). |
| Auto-detect available updates/DLC for installed titles | "3 updates available" badge — proactive, not manual hunting. | MEDIUM | Cross-reference installed list (already enumerable) against the catalog by derived title IDs + version compare. Pure `core/` logic → doctest-covered. |
| Unified manage screen (installed + available in one place) | Tinfoil separates shop from title management; thomaz fuses them. | MEDIUM | Composition of the two list sources above. |
| Resume across app restarts | Long downloads survive a crash/quit. | MEDIUM | Persist partial files + queue manifest; on launch, offer to resume. |

### Anti-Features (Commonly Requested, Often Problematic)

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Bundle/default a content shop, or ship a curated source list | "Make it work out of the box." | Turns thomaz from a neutral client into a content distributor — the exact legal line PROJECT.md draws. | Ship **empty**; user pastes their own URL. No defaults, no link list, no discovery. |
| Act as a server / host / re-share installed titles | "Let me share with friends." | Distribution liability; out of the client-only charter. | Strictly download-and-install. No outbound serving. |
| Built-in title-key / prod.keys management or key derivation | NSZ/encrypted-NCA handling needs keys. | Key handling is legally fraught and a support sink; users already have keys for their CFW. | Rely on the console's existing keys/`prod.keys` already present for CFW; do not bundle, fetch, or generate keys. |
| Google Drive / 1Fichier / Dropbox / FTP / USB-MTP protocol zoo | Tinfoil supports them all. | Each is a maintenance and auth burden; OAuth/API-key flows on a console are painful; long tail of breakage. | Support **HTTP/HTTPS + local SD/USB** only in v1.2. Defer cloud protocols. |
| Block-compressed NSZ "mount and play without install" | Saves disk by running compressed. | Deep NCM/FS integration; even Awoo skips it. | Decompress-on-install only. Support both solid and block NSZ as *input*, but always fully install. |
| XCI gamecard-style "trim/convert" tooling | Power users want format conversion. | Scope creep into a dump/convert tool (DBI/nxdumptool territory). | Install-only. No conversion/dumping. |
| Parallel/concurrent installs | "Faster." | NCM content storage isn't safe for concurrent writers; risks corruption/brick. | Sequential queue with concurrent *downloads* at most, serialized *installs*. |
| Auto-install detected updates without consent | "Just keep me current." | Surprise multi-GB downloads, version pinning broken, no user control. | Detect + badge + one-tap install, always user-initiated. |

---

## Feature Dependencies

```
Link remote source (URL + auth)
    └──requires──> curl HTTP client (EXISTS: http_client_curl)

Parse Tinfoil index JSON
    └──requires──> Link remote source
            └──feeds──> Catalog browse (list)
                            └──enhances──> Cover-art browse (EXISTS: game grid + image_transcode + titledb)

Install base title (NCM/NS/ES + NCA handling)
    └──requires──> Package acquisition (download OR local file)
    └──shares-pipeline──> Install update + DLC
    └──requires──> NSZ/XCZ zstd decompression (zstd EXISTS on link line via libarchive)

List installed titles (EXISTS: title_service_switch / nsListApplicationRecord)
    └──feeds──> Auto-detect updates/DLC  <── also requires ── Catalog browse
    └──feeds──> Uninstall
    └──enhances──> Free space display (EXISTS: system_status/storage_format)

Download/install queue (resumable)
    └──requires──> Install pipeline + Package acquisition
    └──requires──> HTTP Range support (curl) + persisted queue manifest

One-tap source sync
    └──requires──> Link remote source + cloud account (EXISTS: JWT auth + saves sync pattern)
```

### Dependency notes

- **Install update/DLC reuses the base-install pipeline** — build base install first; update/DLC is then classification + the same NCM writes.
- **Auto-detect updates/DLC needs both** the installed-title list (exists) and a parsed catalog; it is pure comparison logic → belongs in `core/` with doctest coverage.
- **The queue depends on the install pipeline existing** — don't build queue UX before single-install works end to end.
- **Cover-art browse enhances (not blocks) catalog browse** — ship a working list first, then layer the grid.
- **One-tap sync mirrors the existing cloud-saves architecture** (JWT auth, optimistic-locking sync, API route + Postgres model) — lowest-risk new server work.

---

## MVP Definition

### Launch With (v1.2 core)

- [ ] Link a remote HTTP/HTTPS source with auth (basic-auth URL, custom header, referrer) — interop foundation
- [ ] Parse Tinfoil index JSON (`files`/`titledb`/`success`/`referrer`/`headers`) — ecosystem compatibility
- [ ] Catalog browse (list first) — see what's available
- [ ] Install base title (NSP) end to end — the core operation
- [ ] Install update + DLC — parity expectation
- [ ] NSZ/XCZ decompression on install — modern packages are NSZ; NSP-only feels broken
- [ ] Install from local SD file — offline path
- [ ] List installed titles with version + DLC + size (mostly reuse) — half the value
- [ ] Free space (NAND + SD) — fit check
- [ ] Uninstall base/update/DLC — lifecycle completeness
- [ ] Per-install progress + cancel + NCA sanity warning — trust + control

### Add After Validation (v1.x)

- [ ] Cover-art/metadata browse grid — once list-browse + install are proven
- [ ] Resumable download/install queue — once single install is rock-solid
- [ ] Auto-detect updates/DLC badging — once catalog + installed-list both exist
- [ ] One-tap source sync to cloud account — once local source-config storage works
- [ ] USB storage install source — after SD path is solid

### Future Consideration (v2+)

- [ ] mTLS client-cert auth (`clientCertPub`/`clientCertKey`) — niche
- [ ] `directories` recursion / nested sub-shops — if users hit flat-index limits
- [ ] Resume-across-restart persistence — after in-session resume proven

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Link source + parse index + auth | HIGH | MEDIUM | P1 |
| Install base (NSP) | HIGH | HIGH | P1 |
| Install update + DLC | HIGH | MEDIUM | P1 |
| NSZ/XCZ decompression | HIGH | HIGH | P1 |
| List installed + version/DLC/size | HIGH | LOW (reuse) | P1 |
| Free space display | MEDIUM | LOW (reuse) | P1 |
| Uninstall base/update/DLC | HIGH | MEDIUM | P1 |
| Install from local SD | MEDIUM | MEDIUM | P1 |
| Per-install progress + cancel | HIGH | MEDIUM | P1 |
| Catalog list browse | HIGH | LOW | P1 |
| Cover-art browse grid | HIGH | MEDIUM | P2 |
| Resumable download/install queue | HIGH | HIGH | P2 |
| Auto-detect updates/DLC | HIGH | MEDIUM | P2 |
| One-tap source sync to cloud | MEDIUM | MEDIUM | P2 |
| USB source | MEDIUM | MEDIUM | P2 |
| mTLS / directories recursion / restart-resume | LOW | MEDIUM | P3 |

---

## Competitor Feature Analysis

| Feature | Tinfoil | Awoo Installer | DBI | Goldleaf | thomaz approach |
|---------|---------|----------------|-----|----------|-----------------|
| Remote shop (JSON index) | Yes (defines the format) | Yes (URL/gdrive) | No (MTP/local focus) | Limited | Read the Tinfoil index format; HTTP/HTTPS only |
| Local SD install | Yes | Yes (browse anywhere) | Yes | Yes | Yes |
| USB/MTP install | No | No | **Yes (MTP)** | USB | USB SD-style mount (defer); no MTP |
| NSP/XCI | Yes | Yes | Yes | Yes | Yes |
| NSZ/XCZ | Yes | Yes (not block-compressed) | **Yes (incl. block)** | Partial | Yes (solid + block as input; install-only) |
| Auto-update detection | App self-update only | Self-update on startup | Removes old updates on install | No | **Catalog-wide update/DLC detection for installed titles** |
| Cover-art browse | Minimal | List | Utilitarian | File-manager | **Cover-art grid (key differentiator)** |
| Install queue w/ resume | Weak | Weak | Sequential | No | **Resumable queue w/ progress + cancel (key differentiator)** |
| Installed-title management | Limited | No | Yes | **Yes (tickets/titles/users)** | Yes (version/DLC/size/free space) |
| Cloud-synced config | No | No | No | No | **One-tap source sync via thomaz account (unique)** |
| Hosts content | No | No | No | No | **No (explicit charter)** |

---

## Policy-sensitive flags (keep the requirement-definer honest)

1. **Ship with zero default sources.** No bundled shop, no curated list, no discovery feature. The user pastes their own URL. This is the line between "client" and "distributor."
2. **No content hosting / re-serving** — download-and-install only; no outbound sharing of installed titles.
3. **No key bundling, fetching, or derivation** — rely on keys already present on the user's CFW console; do not ship `prod.keys` or title keys.
4. **Frame all copy as user-supplied-server.** UI strings and docs should say "your server / your files," never imply thomaz provides content.
5. **NCA signature warning, not bypass** — surface "this package was modified / unsigned" warnings (brick protection), consistent with Awoo; never silently install tampered packages.
6. **Cloud-stored source config holds credentials** — the synced source blob may contain auth headers/passwords; treat as sensitive (encrypt at rest, owner-scoped like the hardened save blobs in v1.0).

---

## Existing-infra leverage summary (for the requirement-definer)

| New capability | Existing thomaz asset it builds on | Net new work |
|----------------|-------------------------------------|--------------|
| Download packages + auth | `platform/http_client_curl` (TLS, cancel-on-teardown) | Range/resume, header threading |
| Index JSON parse | `core/themes/themezer_json` pattern (pure parser + doctest) | New schema parser |
| NSZ/XCZ decompression | libarchive **with zstd+lz4 already on link line** (`CMakeLists.txt`); `themes/nca_extract_switch` for NCA handling | zstd NCA decompress path, NCA install writes |
| Install / uninstall (NCM/NS/ES) | `nca_extract_switch` (same libnx API family) | NCM place + ticket/cert install + app-record push + delete |
| List installed + version + DLC | `platform/title_service_switch` (`nsListApplicationRecord`, `nsListApplicationContentMetaStatus`, `nsGetApplicationControlData`) — **already enumerates this** | Surface DLC count + per-content size |
| Free space / size formatting | `system_status`, `core/storage_format` | Wire into install fit-check |
| Cover-art grid browse | `app/game_list_activity`, `game_panel`, `platform/image_transcode` | Bind to catalog source |
| One-tap source sync | JWT auth + `saves` cloud-sync (optimistic locking, owner-scoped blobs) + Fastify/Prisma routes | New `Source` model + route |
| Async UI safety | `ThomazActivity`/`runAsync` base, `async_guard` | Reuse for long installs |

---

## Sources

- [Tinfoil — Custom Index docs](https://blawar.github.io/tinfoil/custom_index/) (HIGH — official index format)
- [Tinfoil — Network Install docs](https://blawar.github.io/tinfoil/network/) (HIGH — categories New Games/Updates/DLC, protocols)
- [TinfoilWebServer README (Myster-Tee)](https://github.com/Myster-Tee/TinfoilWebServer/blob/master/README.md) (HIGH — server-side index, `[titleid]` filename convention, auth, allowed extensions)
- [Awoo Installer (Huntereb)](https://github.com/Huntereb/Awoo-Installer) (HIGH — NSP/NSZ/XCI/XCZ install, NCA signature verification, startup update check, brick-warning)
- [DBI vs Awoo/NSUSBLoader discussion (GBAtemp)](https://gbatemp.net/threads/whats-the-difference-between-dbi-vs-nsusbloader-awooinstaller.603255/) (MEDIUM — MTP, block-NSZ, remove-old-updates behavior)
- [NSZ compressor/decompressor (nicoboss)](https://github.com/nicoboss/nsz) (HIGH — zstd, solid vs block compression, installer support matrix)
- [Title-ID derivation: NUT/Tinfoil naming convention](https://github.com/tiliarou/tinfoil-1) (MEDIUM — base/+0x800 update/+0x1000 DLC rule)
- thomaz source (HIGH, primary): `source/platform/title_service_switch.cpp`, `source/platform/themes/nca_extract_switch.cpp`, `source/platform/http_client_curl.cpp`, `source/platform/image_transcode.cpp`, `source/platform/mods/libarchive_extractor.cpp`, `CMakeLists.txt` (zstd/lz4 link line)

---
*Feature research for: Switch game-management client (Tinfoil-style installer)*
*Researched: 2026-06-06*
