# Milestones

## v1.1 Switch-Only Simplification (Shipped: 2026-06-06)

**Phases completed:** 3 phases, 8 plans, 7 tasks

**Requirements:** 8/8 v1.1 requirements complete (SIMPL-01..03, BUILD-01..03, DOC-01, VERIF-01)

**Known deferred items at close:** 5 (orthogonal UI quick-tasks, 4 already committed; see STATE.md Deferred Items)

**Key accomplishments:**

- CMakeLists.txt is now Switch-only — both PLATFORM_DESKTOP branches (link + packaging) and the dual-target comments are gone, and the two desktop helper scripts are deleted, with the Switch link/.nro/hactool paths preserved exactly.
- The desktop-stripped tree builds clean for Switch and runs on real hardware — `build_switch/thomaz.nro` (7.7 MB) was produced from a from-scratch build and launched on a physical Switch via nxlink.
- README now describes a Switch-only tree built via `scripts/build-switch.sh` and verified by the two single-target gates — every stale desktop-build instruction is gone.
- Both single-target gates pass together: host doctest 209/209 (with the retained test double) and a clean from-scratch Switch build producing `build_switch/thomaz.nro` (7.7 MB).

---

## v1.0 Hardening (Shipped: 2026-06-05)

**Phases completed:** 4 phases, 16 plans, 32 tasks

**Known deferred items at close:** 3 (hardware-only UAT/verification — see STATE.md Deferred Items)

**Requirements:** 18/18 v1 requirements complete (RM-01..04, SEC-01..03, CONC-01..03, DEBT-01..04, TEST-01..04)

**Key accomplishments:**

- Deleted posts/feed/users routes and @fastify/static from the Fastify API, closing SEC-01 save-blob public exposure and removing 415 lines of community-only code
- IAuthClient interface extracted from IFeedClient; 8 community-feed client files deleted; desktop build green with zero errors.
- Post/Like/Comment Prisma models and tables dropped via migration 20260604233032_remove_community_models; Vitest suite green (6/6) and desktop build clean — Phase 1 complete
- RevokedToken Prisma model + applied migration + envToLogger pino map with header redaction lays the DB and logging foundation for Phase 2 security plans
- jti-bearing access tokens with Postgres-backed revocation blocklist: logout now invalidates tokens, pre-deploy tokens pass unblocked, and DB outages fail open
- TEST-01 (save-blob 404 guard), TEST-02 (revision_required branch), and SEC-02 revoked-token behavior locked behind regression tests; all 14 Vitest tests pass silently
- Extracted ensure_parent_dirs and copy_tree from 7 duplicated call-sites into a single thomaz::fs_util POSIX utility with D-05 equivalence gate doctest; 175 tests pass green.
- Pure curl-free tls_policy(bool) seam extracted to host-compilable header; apply_curl_tls refactored to route both branches through it; process-global insecure latch added; TEST-03 doctest covers fail-safe {0,0} and secure {1,2} branches on host
- Persistent red warning Label injected into every activity's AppletFrame header via shared install_tls_warning_banner helper gated on the tls_is_insecure() latch, with translated warning strings in 5 locales
- std::atomic<bool> cloudBusy with documented threading contract: all 10 read/write sites in save_detail_activity converted to .load()/.store(), check-then-set semantics preserved, alive member untouched (S2), test suite green
- ThomazActivity base class with template runAsync and thomaz::core::run_if_alive; alive + cancelled shared_ptr guards; TEST-04b dropped-callback doctest passing under C++17
- Four activities (game_list, save_manager, save_detail, mod_browser) migrated from brls::Activity to ThomazActivity in a single pass per file: alive member removed, 12 async sites migrated to runAsync, 11 C-style casts replaced with null-guarded dynamic_cast; desktop build clean with zero new warnings
- Nine activities (mod_detail, clear_cheats, auth, theme_browser, cheat_detail, settings, theme_detail, mod_manager) migrated to ThomazActivity; alive member removed from all 9 headers; 14 async sites across 8 files migrated to runAsync; mod_manager base-swap-only (no brls::async); desktop build clean with zero new warnings; whole-app brls::async count in activity cpps = 0
- Cooperative curl abort across both curl surfaces: in-flight mod downloads and cloud-save transfers abort when their activity is destroyed via shared_ptr<atomic<bool>> + CURLOPT_XFERINFOFUNCTION
- Host doctests guard the cloud-save upload conflict-resolution composition (classify->plan_push) by asserting conflict, clean-push, and new-slot outcomes against the real save_sync.cpp implementation.
- theme_download.hpp/cpp

---

## v0.5.0 Theme Extraction (Shipped: 2026-06-05, partial)

**Phases completed:** 2 of 4 — Phases 1-2 (9 plans); Phases 3-4 carried forward

**Scope note:** Shipped the on-device theme extraction *engine* and released as app version 0.5.0 (tag `v0.5.0`). Built on the `workspace/temas` line and merged to main. Phase 3 (theme-UI integration — `extract_state_store` + a theme-browser "Extrair layouts do firmware" action) and Phase 4 (optional Home-menu forwarder) were planned but NOT built — carried forward.

**Requirements:** 7/12 complete (EXTRACT-01..04, TAKEOVER-01/02); INTEG-01..05 + TAKEOVER-03 pending (Phases 3-4).

**Known deferred items at close:** on-hardware extraction verification (no physical Switch this session — plans 01-05 / 02-04 Task 2); Phase 3 theme-UI integration; Phase 4 forwarder.

**Key accomplishments:**

- Re-vendored the exelix hactool fork + Mbed TLS 2.28.10 (CMAC) as Switch-only static libs with correct dual-mbedtls link order
- Platform-neutral `key_loader` deriving the SPL NCA header key on-device from PUBLIC Atmosphère 1.7.1 key sources — no user `prod.keys`, derived key never logged (EXTRACT-04)
- hactool facade for in-memory, `/lyt/`-filtered NCA RomFS extraction keyed by the SPL-derived header key
- `__SWITCH__`-guarded extraction entry gated on `AppletType_Application` with a desktop no-op fake; PT hold-`R` title-takeover docs + THIRD_PARTY.md key-source provenance (TAKEOVER-01/02)
- Neutral, libnx-free `szs_validate` (Yaz0+SARC) with host doctests covering valid/wrapped/garbage/short buffers (D-04)
- `extract_all_base_layouts()`: one privileged session pulls every `/lyt/*.szs` from qlaunch (ResidentMenu/Entrance/Flaunch/Set/Notification/common), Psl, and MyPage into the flat `/themes/systemData/` layout `cfw_paths` expects (EXTRACT-01..03)
- Dropped the misleading "install via NXThemes Installer" hint from the theme download flow

**Carried forward to a future milestone:**

- Phase 3 — first-class "Extrair layouts do firmware" action in the theme UI (already-extracted/re-extract state, fw-version record, `base_missing` unblock, success/failure messaging) — INTEG-01..05
- Phase 4 (optional) — installable Home-menu forwarder for one-tap Application-mode launch — TAKEOVER-03
- On-hardware extraction verification

---
