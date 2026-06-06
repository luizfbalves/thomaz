# Pitfalls Research

**Domain:** On-device Nintendo Switch title install/uninstall + Tinfoil-style content client (libnx ncm/ns/es), added to an existing Borealis/libnx homebrew app (thomaz)
**Researched:** 2026-06-06
**Confidence:** HIGH for NCM/install-sequence and codebase-integration pitfalls (switchbrew + Awoo/Tinfoil source + direct code read); MEDIUM for NSZ/zstd memory specifics and applet-mode thresholds (community + tool docs, version-dependent); HIGH for legal/policy framing.

> **Scope note for the roadmapper:** this milestone is a *content client only*. Every pitfall below assumes thomaz writes into the live system content database (NAND/SD) via ncm/ns/es. Unlike cheats/mods/themes/saves — which write app-private files — a bug here can corrupt the console's installed-titles database, brick the HOME menu's software list, or leave un-deletable orphan titles. Treat the content-DB-corruption and atomicity/resume pitfalls as the highest-impact items in the whole milestone.

---

## Critical Pitfalls

### Pitfall 1: Partial install corrupting the NCM content-meta database (orphaned content / un-deletable titles)

**What goes wrong:**
The NSP install sequence is not atomic. The correct order is: `CreatePlaceHolder` → `WritePlaceHolder` (chunked) → `Register` (move placeholder → registered NCA) → `ContentMetaDatabase.Set` → `ContentMetaDatabase.Commit` → `nsPushApplicationRecord`. If the process is interrupted (power loss, crash, user exit, applet suspend) at the wrong step you get an inconsistent state:
- Interrupted before `Register`: orphaned placeholder NCAs consuming space, invisible to the user.
- `Register` done but `Commit` not done: registered NCAs on disk with **no** meta-DB record → wasted space, no way to see/delete via HOME menu.
- `Commit` done but `nsPushApplicationRecord` not done: content + meta exist but the title does not appear on HOME, or appears corrupt.
- Worst case: a half-written meta-DB `Set` that is committed → the **imkvdb** content-meta database itself is inconsistent, which can make *other* installed titles fail to launch or fail to delete.

**Why it happens:**
Developers treat install as "write all the NCAs then poke the DB" and skip the commit ordering, or they commit meta before all NCAs are registered, or they don't push the application record at all. There is no built-in transaction/rollback — atomicity must be implemented by the app.

**How to avoid:**
- Register **all** NCAs for a title (every content_id in the CNMT) *before* the meta-DB `Set`/`Commit`. Never commit meta that references content you haven't fully registered.
- Do `nsPushApplicationRecord` only after meta is committed; build the full ContentStorageRecord list and push it as one record.
- On *any* failure mid-install, run a rollback: `DeletePlaceHolder` for staged placeholders, `Delete` for already-registered NCAs of this title, and do **not** commit the meta-DB. Treat the meta `Commit` as the single linearization point.
- After a known-interrupted/aborted install, run `CleanupAllPlaceHolder` and a content/meta reconciliation pass on next launch.
- Read the CNMT first and validate it lists content you actually have before touching NCM.

**Warning signs:**
Titles that show as "corrupt data" on HOME; titles that can't be deleted from System Settings; free space "leaking" after failed installs; `ncmContentMetaDatabaseCommit` returning success while NCAs are missing.

**Phase to address:**
Core install-engine phase (the phase that first writes to NCM). This is the phase that *must* ship with rollback + a startup reconciliation/cleanup pass, not a later hardening phase.

---

### Pitfall 2: No resume / power-loss safety on the download+install queue

**What goes wrong:**
A multi-GB install is interrupted (sleep, applet eviction, dock change, power loss, curl abort on activity teardown). On restart the app either (a) re-downloads from byte 0, or (b) resumes the download but re-registers content that's already partially staged, or (c) leaves placeholders that are never cleaned. Resumability is a headline feature of this milestone ("resumable download/install queue"), so getting this wrong is a requirement failure, not just a polish gap.

**Why it happens:**
The existing `download_file` (`source/platform/mods/mod_download.cpp`) opens the destination with `"wb"` (truncate) and deletes the file on any non-OK result — it has **no** resume/`CURLOPT_RESUME_FROM_LARGE` path and treats a cooperative abort as "delete the partial." That is correct for a small mod archive but wrong for a resumable multi-GB title download. NCM `WritePlaceHolder` *can* resume by offset, but only if the app persists how many bytes were written per content_id.

**How to avoid:**
- Persist queue state to SD (one record per content_id: URL, expected size, bytes written, placeholder_id, target storage) so resume survives a full app restart, not just an in-session retry.
- For downloads: support HTTP range/`Resume-From` and verify the server honors `Accept-Ranges` (fall back to full re-download if not). Validate total bytes == advertised Content-Length before registering (the existing length check is a good base — extend it to the resumed case).
- For staging: write directly into the NCM placeholder by offset and persist the last committed offset; on resume continue `WritePlaceHolder` from that offset rather than recreating the placeholder.
- Distinguish "user cancelled (clean up)" from "interrupted (keep for resume)" — the current model collapses both into delete-the-file.

**Warning signs:**
Resume re-downloads from 0; placeholders accumulate after sleep/abort; a resumed install registers duplicate or zero-length NCAs; queue state lost on app restart.

**Phase to address:**
Queue/resume phase — but the *persistence schema* (per-content_id offset + placeholder id) must be designed in the core install-engine phase so resume can be built on top without redesign.

---

### Pitfall 3: Ticket / cert handling — personalized vs common, and silently skipping import

**What goes wrong:**
Titles whose NCAs are encrypted with a title key require a **ticket** imported via `es` (`esImportTicket`) plus the cert chain. Two failure modes: (1) the app imports a *personalized* ticket (tied to a specific console/account) onto a different console — these carry console-identifying data and importing other people's personalized tickets is both a privacy and a ban-risk concern; (2) the app treats ticket import failure as fatal and aborts an otherwise-installable title, or the opposite — silently skips the ticket so the title installs but won't launch ("missing ticket"). Awoo/Tinfoil deliberately warn "Ticket installation failed! This may not be an issue" and proceed, because some content is standard-crypto and needs no ticket.

**Why it happens:**
Ticket logic is subtle: not every NCA needs a ticket (standard crypto vs titlekey crypto), the rights_id in the NCA header tells you whether one is needed, and personalized vs common is a property of the ticket's signature type/data. Devs either hard-require tickets (breaks no-ticket content) or ignore them (breaks titlekey content), and rarely strip/normalize personalized → common.

**How to avoid:**
- Inspect each NCA's rights_id: if all-zero, no ticket needed — do not import one. If non-zero, a ticket for that rights_id is required to launch.
- Prefer **common** tickets. If a ticket is personalized, surface that to the user; do not blindly import third-party personalized tickets (privacy/ban surface). Consider refusing or warning rather than silently importing.
- Import the cert chain alongside the ticket. Don't make ticket import fatal for no-ticket-needed titles, but DO make it fatal (with a clear "missing ticket" message) when the rights_id requires one and import failed.
- Be explicit that successful install ≠ launchable: a title with an un-imported required ticket installs cleanly and then black-screens.

**Warning signs:**
Installed title black-screens / "could not start software"; "Failed to import ticket" treated as success and the title is broken; importing tickets for titles that have all-zero rights_id (unnecessary, and pollutes the ticket DB).

**Phase to address:**
Core install-engine phase (ticket import is part of the install transaction). Personalized-ticket policy decision belongs in the same phase as a security/privacy requirement.

---

### Pitfall 4: In-memory buffering of multi-GB downloads via the existing HTTP client (OOM)

**What goes wrong:**
`thomaz::HttpResponse.body` is a `std::string` that buffers the **entire** response in RAM (`source/platform/http_client.hpp`). The catalog-fetch / auth flows reuse `IHttpClient::request`. If a developer reuses that same path to fetch a title NCA (multi-GB), the app tries to allocate the whole file in memory and OOM-crashes — especially in applet mode (~440 MB cap, see Pitfall 5).

**Why it happens:**
The catalog and the content live on the *same* user server, so it's natural to reach for the existing JSON HTTP client for the file download too. The buffered-body design is correct for small JSON but catastrophic for content.

**How to avoid:**
- Hard rule: content bytes NEVER go through `IHttpClient::request`/`HttpResponse.body`. Use a streaming sink (the `download_file` write-callback pattern in `mod_download.cpp`) that writes straight to the NCM placeholder or to SD, never accumulating in a `std::string`/`std::vector`.
- Use `IHttpClient` only for the JSON index, auth, and small metadata (CNMT/cover thumbnails).
- Cap any in-memory buffer (cover art, CNMT) with an explicit size limit so a hostile/large server response can't OOM the app.

**Warning signs:**
RAM usage scales with file size; crash on the first large title; cover/catalog code and download code share the same response type.

**Phase to address:**
Content-server/transport phase — establish the streaming-download seam separate from the JSON client *before* the install engine consumes it.

---

### Pitfall 5: NSZ/NCZ zstd decompression blowing the applet-mode memory budget

**What goes wrong:**
NSZ/NCZ content is zStandard-compressed NCA data. **Solid** compression (the NSZ default) has no random access and must be decompressed as one continuous stream; **block** compression splits into independent blocks. A naïve decompressor that buffers a whole decompressed NCA section, or uses a large zstd window/streaming context, can exceed the ~440 MB applet-mode budget and OOM. There was historically a real memory leak in NSZ decompression fixed by moving to the simple block-decompress API.

**Why it happens:**
zstd streaming contexts plus a full-section output buffer are easy to size wrong; solid-compressed files tempt a "decompress to a temp buffer then write" approach that holds the whole decompressed chunk in RAM at once.

**How to avoid:**
- Stream-decompress: read a bounded compressed chunk, decompress into a small fixed output buffer, write that buffer straight into the NCM placeholder by offset, repeat. Never hold a whole decompressed NCA/section in RAM.
- Bound the zstd window/streaming-context size and reuse one context; free it deterministically (the leak history is a warning).
- Handle the NCZ structure correctly: header, then the zstd stream begins after the header and the first 0x4000 bytes are stored decrypted/uncompressed — get that offset boundary right or you corrupt the NCA.
- Prefer/handle block mode for bounded memory; treat solid mode as "still must stream, never fully buffer."

**Warning signs:**
OOM only on NSZ/NCZ (not plain NSP); memory grows with title size during decompress; intermittent failures that disappear in title-takeover (full-RAM) mode.

**Phase to address:**
NSZ-support phase (likely after plain-NSP install works). Flag this phase as needing **deeper research/spike** — it's the highest technical-uncertainty item.

---

### Pitfall 6: Running in applet mode (~440 MB) instead of full-RAM title-takeover

**What goes wrong:**
Launched from the Album icon, homebrew gets only ~440 MB. Installs (especially NSZ decompress + curl buffers + Borealis UI + image cache) can exceed that and crash mid-install — which then triggers Pitfall 1 (corrupt content DB). The same code runs fine when launched via title-takeover (hold-R on a game, ~3 GB+).

**Why it happens:**
Most users launch homebrew from the Album applet by default; devs test in title-takeover and never see the OOM.

**How to avoid:**
- Detect applet vs full-application mode at runtime (`appletGetAppletType`) and either (a) warn the user that installs require title-takeover, or (b) keep peak memory bounded enough to install even in applet mode (preferred — strict streaming everywhere, small fixed buffers).
- Document the launch recommendation; show a one-time banner if running in applet mode before a large install.
- Free the image/cover-art cache aggressively during an active install.

**Warning signs:**
"Works for me" installs that crash for users; crashes correlated with how the app was launched; failures that vanish at smaller title sizes.

**Phase to address:**
Content-server/transport phase (set the memory-discipline contract) + a runtime check surfaced in the install/queue UI phase.

---

### Pitfall 7: NAND vs SD destination, free-space checks, and the exFAT 4 GB split-file trap

**What goes wrong:**
Three linked mistakes: (1) installing to the wrong `NcmStorageId` (System NAND vs SdCard) — NAND is tiny and filling it can destabilize the system; (2) not checking free space on the chosen storage *before* staging, so the install fills the card mid-write and fails non-atomically (→ Pitfall 1); (3) the FAT32 4 GB limit — a single NCA >4 GB cannot exist as one file on FAT32, and even on SD the system may store oversized content as a split "archive-bit folder" (`00`,`01`,`02`… inside a `.nca`/`.nsp` folder with the archive bit set). Reading *local* SD/USB NSP/XCI that are split this way requires concatenating the parts in order and treating the archive-bit folder as one logical file; missing this means local split installs silently fail or read garbage.

**Why it happens:**
Devs default to SD and assume one-file-per-NCA; they forget FAT32 is common and that the Switch's own split-file convention (archive bit + numbered parts) must be reassembled on read; free-space is checked too late or not at all.

**How to avoid:**
- Let the user choose NAND vs SD; default to SD; warn loudly before NAND installs.
- Query free space (`ncmContentStorageGetFreeSpaceSize` / fs free space) and the largest single-file limit of the target filesystem *before* creating any placeholder; refuse early with a clear message.
- For local sources: detect archive-bit split folders and present them as a single file; read parts `00..NN` in order. Don't assume exFAT — handle FAT32's 4 GB ceiling.
- When the system auto-splits a >4 GB NCA on install, ensure your write/offset logic survives the split (let NCM handle splitting; don't hand-roll file writes that assume a single contiguous file).

**Warning signs:**
Install fails only on large titles; local split NSP/XCI in a `.nsp` folder not recognized; install fills NAND; "no space" errors only after gigabytes are already written.

**Phase to address:**
Destination/free-space handling in the core install-engine phase; split-file reading in the local-SD/USB-source phase.

---

### Pitfall 8: Concurrency — breaking the existing `runAsync`/`alive`/`cancelled` contract with NCM handles

**What goes wrong:**
The existing model (`source/app/thomaz_activity.hpp`, `source/core/async_guard.hpp`) runs work on the Borealis async pool; the worker **must not touch `this`**, and `onSync` runs on the UI thread only if `alive` is still set. The destructor sets `*cancelled=true` and curl checks it in `XFERINFOFUNCTION` to abort. A multi-step install (download → decompress → NCM register → commit) is long-running and holds NCM/NS/ES service handles. Two new hazards: (1) if install work is launched with the existing per-activity `runAsync` and the user pops the activity mid-install, `cancelled` aborts the curl transfer — but the *NCM transaction* is now interrupted mid-flight, and the open ncm/ns service handles may be closed by the dtor while the pool thread is still using them → use-after-close / data abort; (2) two installs running concurrently (or an install plus a uninstall) both mutating the content-meta DB → DB corruption (the imkvdb is not concurrency-safe across uncoordinated writers).

**Why it happens:**
The `runAsync` model was designed for short fetch-then-update-UI tasks (CONC-02/CONC-03), not for a long transactional pipeline that owns system resources and must clean up on cancel. Cancelling a curl download is safe; cancelling halfway through an NCM commit is not.

**How to avoid:**
- Run the install/queue as a **single serialized worker** (one in-flight install at a time; uninstall mutually exclusive with install) — mirror the existing `cloudBusy` `std::atomic<bool>` gate (CONC-01) with an `installBusy`/queue-owns-the-mutation pattern.
- Decouple the queue's lifetime from any one activity. The download/install engine should own its own `cancelled` flag and its NCM handles, NOT borrow an activity's `alive`/`cancelled` (which die when the user navigates away). Navigating away from the queue UI must NOT tear down an in-progress install.
- On cancel, the engine must reach a **safe rollback point** (finish or abort the current NCM step, run `DeletePlaceHolder`/`Delete`, skip the commit) before releasing handles — cooperative cancellation, not abrupt teardown. Open/close ncm/ns/es handles inside the engine's own scope, never tied to UI object lifetime.
- Keep workers off `this`, as the existing contract requires; marshal progress back via the queue's own state + `brls::sync`, guarded by `run_if_alive` for the *currently shown* UI only.

**Warning signs:**
Crash/data-abort when leaving the install screen mid-install; two installs corrupting the DB; `brls::async` count regressing above 0; NCM calls after `serviceClose`; install "continues" but UI guard already fired.

**Phase to address:**
The install-engine/queue phase — and explicitly call out that the engine must NOT reuse the per-activity `alive`/`cancelled` lifetime for the transactional NCM work. This is the #1 *integration* pitfall with this codebase.

---

### Pitfall 9: Gamecard / version / required-system-version mismatch

**What goes wrong:**
Installing an update without the base, or a DLC without the base/update, or an update whose `RequiredApplicationVersion` doesn't match the installed base, or content whose `RequiredSystemVersion` exceeds the console's firmware → black screen, "software needs an update," or non-launching titles. Also: installing a base title that overlaps a gamecard-resident title can confuse the version the system reports.

**Why it happens:**
CNMT carries meta type (Application/Patch/AddOnContent/SystemUpdate), the title's required versions, and dependencies; devs install whatever the index points at without validating prerequisites or firmware.

**How to avoid:**
- Parse the CNMT meta type and required versions; before installing an update/DLC, verify the base (and required update) are present, and surface a clear message if not.
- Check `RequiredSystemVersion` against the running firmware and warn/refuse rather than installing un-launchable content.
- Don't blindly install a `SystemUpdate` meta type — that's firmware and is out of scope/dangerous for a content client.
- After installing base+update+DLC, push one coherent application record reflecting the highest installed version.

**Warning signs:**
"A software update is required" after install; DLC not appearing; base launches but update doesn't; firmware-too-low black screens.

**Phase to address:**
Update/DLC-install phase (after base-title install works); CNMT meta-type validation in the core install-engine phase.

---

### Pitfall 10: TLS/auth for arbitrary user-supplied servers

**What goes wrong:**
The content server is *user-supplied* (HTTP/HTTPS, optional auth). Pitfalls: (1) following the existing fail-closed TLS policy (SEC-03/D-06a: refuse on CA-probe failure) is correct, but users will point at self-hosted servers with self-signed/private-CA certs or plain HTTP and expect it to "just work" — silently disabling verification to accommodate them re-opens MITM on the very bytes being written into the system DB; (2) leaking the user's auth token to the wrong host on HTTP→HTTPS or cross-host redirects (curl `FOLLOWLOCATION` is on in `download_file`); (3) trusting the server's advertised file size / hash without verifying NCA header signatures, letting a malicious server feed modified NCAs.

**Why it happens:**
"It's the user's own server" lulls devs into relaxing TLS and not verifying content. But the bytes land in the *system* content DB, so integrity matters more here than for a mod archive.

**How to avoid:**
- Keep fail-closed TLS (consistent with SEC-03). For self-signed servers, require an explicit, scary opt-in per server (not a global "disable TLS") and never send auth credentials over plain HTTP.
- Strip the `Authorization` header on cross-host redirects; pin auth to the linked server's origin. Sync server config to the thomaz account over the existing authenticated API only.
- Verify NCA header signatures before `Register` (as Awoo does) and warn on modified content; verify downloaded size/hash against the CNMT, not just the server's claim.
- Validate/parse the JSON index defensively (size caps, type checks) — a malicious index shouldn't be able to OOM or path-traverse.

**Warning signs:**
A "disable TLS verification" global toggle; auth token sent to redirect targets; install proceeds on signature-mismatched NCAs; index parsing without size limits.

**Phase to address:**
Content-server/linking + auth phase; signature verification in the core install-engine phase.

---

### Pitfall 11: Legal/policy framing — staying a client, never a host/distributor

**What goes wrong:**
The feature drifts from "client for a user-supplied server" toward bundling/hosting/distributing content or default-pointing at infringing indexes — turning thomaz from a tool into a distribution vector, with the corresponding legal exposure and likely takedown of the whole project (and its live API at `api.thomaz.baseup.cc`).

**Why it happens:**
Convenience creep: shipping a default catalog URL, caching content on thomaz infrastructure, embedding keys/sigpatches, or syncing *content* (not just server config) to the cloud account.

**How to avoid:**
- Ship **no** default content server, **no** bundled index, **no** keys/sigpatches/content. The user must enter their own server.
- The cloud account syncs only the user's *server configuration* (URL/credentials), never content or catalog data — keep this an explicit boundary in the API design.
- Keep the in-app responsibility framing: thomaz is the client; the user supplies the server; misuse is the user's responsibility. Don't editorialize toward piracy in UI copy.
- Don't proxy content through thomaz infrastructure; downloads go device → user server directly.

**Warning signs:**
A hard-coded default catalog URL; content cached server-side; "popular titles" curation; keys/sigpatches in the repo or romfs; cloud sync payloads containing catalog/content.

**Phase to address:**
Content-server/linking phase (no-default-server, config-only-sync as a hard requirement) and a milestone-wide review gate. This is a real constraint, not boilerplate.

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Reuse `IHttpClient`/`HttpResponse.body` for content downloads | One transport, fast to wire | Guaranteed OOM on real titles; forces a rewrite of the install path | Never for content; fine for JSON index/CNMT/thumbnails only |
| Skip rollback on failed install ("clean it up later") | Ship the happy path sooner | Orphaned placeholders/NCAs + content-DB corruption; un-deletable titles | Never — rollback must ship with the first NCM write |
| Tie the install worker to the activity's `alive`/`cancelled` | Reuses the proven CONC-02/03 model | UAF/data-abort on NCM handles when user navigates away mid-install | Never for the transactional engine; fine for progress-UI updates |
| Allow concurrent installs/uninstalls | Simpler UI, parallel speed | imkvdb content-meta DB corruption | Never — serialize all DB-mutating ops |
| Fully buffer then write each NCA/NSZ chunk | Simpler decompress code | OOM in applet mode; non-streaming | Only for tiny content (<a few MB), never for NCAs |
| Hard-require ticket import for every title | Simpler branching | Breaks no-ticket (standard-crypto) content | Never; gate on rights_id |
| Ship a default content server URL | "It just works" demo | Turns the project into a distribution vector (legal) | Never |

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| libnx `ncm` | Commit meta-DB before all NCAs registered; no rollback | Register all content_ids → `Set` → `Commit` as the single linearization point; rollback on failure |
| libnx `ns` | Forget `nsPushApplicationRecord`, or push before meta commit | Push one coherent application record after meta commit |
| libnx `es` | Import tickets unconditionally / treat failure as fatal | Gate on NCA rights_id; common-ticket preference; surface personalized tickets |
| Existing `download_file` (`mod_download.cpp`) | Reuse as-is for resumable title download | Truncates (`"wb"`) + deletes partial on abort + no resume; add range/resume + keep-partial-on-interrupt path |
| `ThomazActivity::runAsync` | Run the long install on it; let dtor cancel mid-NCM | Engine owns its own lifetime + `cancelled`; activities only render progress |
| `cloudBusy` pattern (CONC-01) | Add a second unsynchronized busy flag for installs | Mirror the documented `std::atomic<bool>` gate as `installBusy`; serialize DB mutations |
| curl `FOLLOWLOCATION` (already on) | Leak `Authorization` across cross-host redirects | Strip auth on origin change; pin to linked server |
| Thomaz cloud API | Sync catalog/content to account | Sync only server config (URL/creds); never content |

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| In-memory full-file buffering | RAM scales with file size | Stream to placeholder by offset | First real title (>~400 MB) in applet mode |
| Non-streaming NSZ decompress | OOM only on NSZ; mem grows during decompress | Bounded zstd context + fixed output buffer | Any solid-compressed NSZ in applet mode |
| Re-download on resume | Multi-GB re-fetched after interruption | Persist per-content_id offset; HTTP range | Every sleep/abort on a large title |
| Cover-art cache unbounded during install | Crash on large catalogs while installing | Cap + flush image cache during active install | Large catalogs + concurrent install in applet mode |
| Double traversal / extra metadata fetches | Slow catalog + install | Reuse parsed CNMT; one pass | Large catalogs (echoes deferred PERF-01/02) |

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| Global "disable TLS verification" toggle for self-hosted servers | MITM injects modified NCAs into the system DB | Per-server explicit opt-in; never send creds over HTTP; keep SEC-03 fail-closed default |
| Trusting server-advertised size/hash; no NCA signature check | Malicious/modified content installed into NAND/SD | Verify NCA header signature before `Register`; verify against CNMT |
| Leaking auth token on redirect | Credential theft by a redirect target | Strip `Authorization` on cross-host redirect; origin-pin |
| Importing third-party personalized tickets | Privacy (console-identifying data) + ban surface | Prefer common tickets; warn/refuse on personalized |
| Unbounded JSON index parse | OOM / path traversal from hostile index | Size caps, type validation, sanitize paths |
| Bundling keys/sigpatches | Legal + integrity | Ship none; rely on the user's CFW environment |

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Cancel deletes a resumable in-progress download | Lost multi-GB progress | Separate "cancel (discard)" from "pause/leave (resume later)" |
| No free-space/firmware check before install | Fails after gigabytes written | Pre-flight checks with early, clear refusal |
| Silent ticket skip → installed-but-won't-launch title | "Why does my game black-screen?" | Detect required ticket; block + explain on missing |
| Leaving the queue screen kills the install | User can't browse while installing | Engine outlives the UI; background queue |
| No applet-mode warning before a big install | Mid-install OOM crash | Detect applet mode; warn/recommend title-takeover |
| Showing only "installed" without version/DLC/source | Can't tell what's missing | List version, update, DLC, size, storage per title |

## "Looks Done But Isn't" Checklist

- [ ] **NSP install:** Often missing rollback — verify an interrupted install leaves NO orphaned placeholders/NCAs and an uncommitted meta-DB; verify startup reconciliation/`CleanupAllPlaceHolder`.
- [ ] **Resume:** Often missing cross-restart persistence — verify resume works after a full app exit, not just in-session retry.
- [ ] **Ticket handling:** Often missing rights_id gating — verify no-ticket titles install without a spurious ticket and titlekey titles refuse cleanly when the ticket is missing.
- [ ] **Streaming:** Often missing the OOM guard — verify peak RAM is flat (not size-proportional) for a multi-GB install, in applet mode.
- [ ] **NSZ:** Often missing bounded decompression — verify a solid-compressed NSZ installs without RAM growth and the zstd context is freed.
- [ ] **Local split files:** Often missing archive-bit-folder reassembly — verify a split `.nsp` folder (`00`,`01`,…) installs correctly on FAT32.
- [ ] **Update/DLC:** Often missing prerequisite + RequiredSystemVersion checks — verify update-without-base and firmware-too-low are caught.
- [ ] **Concurrency:** Often missing serialization — verify two installs / install+uninstall cannot run together and that leaving the screen doesn't abort the NCM transaction.
- [ ] **TLS/auth:** Often missing redirect credential stripping — verify auth isn't sent to redirect targets and self-signed requires explicit opt-in.
- [ ] **Legal:** Verify no default server URL, no bundled index/keys, cloud sync carries config only.

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Orphaned placeholders | LOW | `CleanupAllPlaceHolder` on the storage; run on next launch after a known abort |
| Registered NCAs without meta record | MEDIUM | Reconcile: enumerate registered content vs meta-DB; `Delete` unreferenced NCAs (free space) |
| Corrupt content-meta DB / un-deletable title | HIGH | Often needs external tools (DBI/Goldleaf) or DB rebuild; PREVENT via commit-as-linearization-point + serialization |
| Installed title won't launch (missing ticket) | LOW | Re-import the correct (common) ticket for the rights_id |
| Update/DLC without base | LOW | Uninstall the orphan; install base first; re-push application record |
| Disk filled mid-install | MEDIUM | Roll back this title (placeholders + registered NCAs); add pre-flight free-space check |
| Resume lost on restart | MEDIUM | Restart download; root cause = no persisted per-content_id offset (fix the schema) |

## Pitfall-to-Phase Mapping

> Phase names are indicative; the roadmapper assigns final numbering. Ordering implication: a **Core install engine** must precede update/DLC, NSZ, and queue/resume, and it must ship rollback + reconciliation + serialization from day one.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| 1. Partial install / content-DB corruption | Core install engine | Interrupt at each step → no orphans, uncommitted meta, startup cleanup runs |
| 2. No resume / power-loss safety | Queue/resume (schema designed in core engine) | Kill app mid-install → resumes from persisted offset |
| 3. Ticket/cert (personalized vs common) | Core install engine | No-ticket title installs clean; titlekey title refuses on missing ticket |
| 4. In-memory buffering OOM | Content-server/transport | Flat peak RAM on multi-GB install |
| 5. NSZ/zstd memory | NSZ-support (flag for spike) | Solid NSZ installs with bounded RAM; context freed |
| 6. Applet-mode RAM | Transport + queue UI | Applet-mode detect + warn; install succeeds in applet mode |
| 7. NAND/SD + free space + exFAT 4GB split | Core engine (dest/space) + local-source phase | Pre-flight space check; split `.nsp` folder installs on FAT32 |
| 8. Concurrency vs runAsync/alive/cancelled | Install-engine/queue | No data-abort leaving screen mid-install; installs serialized; `brls::async`==0 |
| 9. Version / RequiredSystemVersion mismatch | Update/DLC phase (meta-type in core) | Update-without-base + low-firmware caught |
| 10. TLS/auth for user servers | Server linking + auth (sig-verify in core) | No auth leak on redirect; self-signed opt-in; sig-mismatch refused |
| 11. Legal/policy (client not host) | Server linking + milestone review | No default server/index/keys; cloud sync = config only |

## Sources

- [NCM services — Nintendo Switch Brew](https://switchbrew.org/wiki/NCM_services) — PlaceHolder/Register/ContentMetaDatabase Set/Commit sequence, CleanupAllPlaceHolder (HIGH)
- [CNMT — Nintendo Switch Brew](https://switchbrew.org/wiki/CNMT) — content meta types, required versions (HIGH)
- [Memory layout — Nintendo Switch Brew](https://switchbrew.org/wiki/Memory_layout) — applet vs application memory (HIGH)
- [libnx ncm.h / ncm.c](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/services/ncm.h) — actual API surface (HIGH)
- [Huntereb/Awoo-Installer install.cpp](https://github.com/Huntereb/Awoo-Installer) — real install flow: ncmContentMetaDatabaseSet/Commit, nsPushApplicationRecord, InstallTicketCert non-fatal, NCA signature verify, storage selection (HIGH)
- [Awoo Installer DeepWiki technical details](https://deepwiki.com/Huntereb/Awoo-Installer/7-technical-details) — NSP/NSZ/NCZ handling (MEDIUM)
- [nicoboss/nsz](https://github.com/nicoboss/nsz) + [NSZ GBAtemp thread](https://gbatemp.net/threads/nsz-homebrew-compatible-nsp-xci-compressor-decompressor.550556/) — NCZ format, solid vs block, 0x4000 offset, decompress memory-leak fix (MEDIUM)
- [splitNSP / XCI-to-Split-NSP](https://github.com/AnalogMan151/splitNSP) + [FAT32 split-file how-to](https://wejn.org/2023/05/split-big-xci-or-nsp-to-store-it-on-fat32/) — archive-bit folder, 4 GB limit, `00/01/02` parts (MEDIUM)
- [Tinfoil ticket types / sigpatches threads (GBAtemp)](https://gbatemp.net/threads/tinfoil-delete-common-personalized-ticket-wth-does-this-mean.519398/) — personalized vs common tickets, missing-ticket symptoms (MEDIUM)
- [Switch games firmware requirement — WikiTemp](https://wiki.gbatemp.net/wiki/Switch_games_firmware_requirement) — RequiredSystemVersion / "software update required" (MEDIUM)
- Direct codebase read: `source/platform/http_client.hpp` (buffered body), `source/platform/mods/mod_download.cpp` (streaming download + cancel + length check), `source/app/thomaz_activity.hpp` + `source/core/async_guard.hpp` (runAsync/alive/cancelled), `.planning/PROJECT.md` (CONC-01/02/03, SEC-03 fail-closed, milestone scope) (HIGH)

---
*Pitfalls research for: on-device Switch title install/uninstall + Tinfoil-style content client (thomaz v1.2 Game Management)*
*Researched: 2026-06-06*
