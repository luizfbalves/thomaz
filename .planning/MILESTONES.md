# Milestones

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
