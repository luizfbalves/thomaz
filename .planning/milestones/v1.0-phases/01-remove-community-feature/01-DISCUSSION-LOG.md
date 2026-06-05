# Phase 1: Remove Community Feature - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-04
**Phase:** 1-Remove Community Feature
**Areas discussed:** Save-blob rollout (later obsoleted), Logout scope (→ Phase 2), Validation limits (obsoleted), Community-feature removal (scope + placement)

---

> Context: this discussion began as `/gsd-discuss-phase 1` for the original "API Security" phase. Mid-discussion the user said the posts feature is gone and all related code should be removed, which restructured the roadmap (removal inserted as Phase 1). Several earlier answers were obsoleted by that pivot; they are logged for the record.

## Save-blob rollout (original Phase 1 — now obsoleted)

| Option | Description | Selected |
|--------|-------------|----------|
| Auto on startup (idempotent) | Server migrates uploads/saves/ → SAVES_DIR on boot | ✓ |
| Manual step on Lightsail | Run cp -r over SSH around deploy | |
| No migration (re-upload) | Start empty, users re-upload | |

**User's choice:** Auto on startup; zero-downtime cp-verify-remove.
**Notes:** OBSOLETED by the removal decision — removing `@fastify/static` means save blobs are no longer served publicly, so no directory migration is needed at all. Blobs stay in `uploads/saves/`.

## Logout scope (→ moved to Phase 2 / SEC-02)

| Option | Description | Selected |
|--------|-------------|----------|
| Current token only | Blocklist only the access token used at logout (by jti) | ✓ |
| All user sessions | Logout-everywhere; revoke all of the user's tokens | |

| Option | Description | Selected |
|--------|-------------|----------|
| Old tokens pass until relogin | Tokens without jti skip the blocklist check | ✓ |
| Force relogin | Invalidate all jti-less tokens at once | |

**User's choice:** Current-token-only revocation; pre-existing 365-day tokens (no jti) pass through until relogin.
**Notes:** Still valid, but belongs to Phase 2 (token revocation), not removal. Preserved in PROJECT.md Key Decisions.

## Validation limits (original Phase 1 — obsoleted)

| Option | Description | Selected |
|--------|-------------|----------|
| Caption 500 chars | Research/CONCERNS suggestion | ✓ |
| JPEG only (keep current) | Magic-byte SOI check | ✓ |

**User's choice:** 500 / JPEG-only.
**Notes:** OBSOLETED — `posts.ts` (the caption + image upload site) is being removed, so VAL-01/VAL-02 moved to Out of Scope.

## Community-feature removal (scope + placement)

| Option | Description | Selected |
|--------|-------------|----------|
| Entire community feature | posts + feed + comments/likes across API, client, DB | ✓ |
| Posts only (keep feed read-only) | Partial removal | |
| Server-side only | Remove API; leave client | |

| Option | Description | Selected |
|--------|-------------|----------|
| Inside Phase 1 (original) | Replace VAL items in the security phase | |
| New dedicated phase before Phase 1 | Removal is large/cross-cutting; clear the ground first | ✓ |
| Let Claude decide placement | | |

**User's choice:** Remove the entire community feature, as a new dedicated phase placed first.
**Notes:** Approved the 4-phase restructure. SEC-01 to be resolved in the removal phase (static removal) and verified in Phase 2 (TEST-01).

---

## Claude's Discretion

- Removal order and commit granularity (planner/executor decide; build/tests green at each step).
- Whether to drop now-unused deps (`@fastify/static`, `@fastify/multipart`) from `api/package.json`.

## Deferred Ideas

- Logout revocation scope → Phase 2 / SEC-02 (decision preserved in PROJECT.md).
- Relocating `session_codec` / `auth_store` out of `feed/` directories → optional later cleanup.
