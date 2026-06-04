# thomaz — Extração de Temas Milestone

## What This Is

thomaz is a Nintendo Switch homebrew hub (Borealis UI, devkitPro NRO + desktop build) that manages cheats, mods, themes, cloud saves, and sysmodules, backed by a Node.js/Fastify + PostgreSQL cloud API. This milestone adds **native firmware layout extraction**: the app extracts the home-menu base `.szs` layouts from the running console into `/themes/systemData/` by itself, removing the dependency on NXThemes Installer for first-time base-layout extraction. This is the implementation of the firmware-extraction path that Phase B (native theme apply) deliberately deferred ("Path B").

## Core Value

A user can apply downloaded themes on a fresh console using thomaz alone — no second app — because thomaz can extract the firmware base layouts on-device, keyless to the user (no `prod.keys` file required).

## Requirements

### Validated

<!-- Existing, working capabilities — do not re-build. -->

- ✓ Cheat fetch + apply via switch-cheats-db (`core/cheat_repository`, `platform/cheat_store`) — existing
- ✓ Mod browse/install from GameBanana with archive extraction (`core/mods`, `platform/mods`) — existing
- ✓ Theme browse via Themezer + **native apply** via vendored exelix engine — Phase B (apply/remove/reboot/active-indicator) — existing
- ✓ Cloud save upload/download/sync with optimistic locking (`core/saves`, `platform/saves`) — existing
- ✓ Sysmodule scan/manage (`core/sysmod`, `platform/sysmod`) — existing
- ✓ Clean-architecture split: pure `core/` host-tested via doctest, `platform/` switch/fake pairs — existing
- ✓ `cfw_paths` / `active_theme_store` / `theme_install` already read base layouts from `/themes/systemData/` — existing (Phase B); this milestone *produces* what they consume

### Active

<!-- This milestone: native on-device extraction of the firmware base layouts. -->

**Extraction engine (Option A — BIS + SPL + hactool, ported from exelix GPLv2)**
- [ ] Re-vendor the hactool fork + custom mbedtls (`MBEDTLS_CMAC_C`) excluded in Phase B
- [ ] Port `key_loader` (`__SWITCH__`): BIS System mount, `lr` title→NCA resolve, SPL header-key derivation
- [ ] Port `hactool` in-memory NCA RomFS extraction with `/lyt/*.szs` filename filter
- [ ] Extract qlaunch (`…1000`: ResidentMenu/Entrance/Flaunch/Set/Notification/common), Psl (`…1007`), MyPage (`…1013`)

**Integration with the app**
- [ ] "Extrair layouts do firmware" action in the theme UI (one-time)
- [ ] Write extracted `.szs` to the canonical `/themes/systemData/` layout that `cfw_paths` expects
- [ ] After successful extraction, `base_missing` no longer blocks "Aplicar Tema"
- [ ] Detect "already extracted" + allow re-extract (e.g. after firmware update); record firmware version

**Title-takeover enablement**
- [ ] Determine + document the title-takeover launch path required for privileged FS/SPL access
- [ ] Detect applet vs Application mode at runtime; show a clear message if extraction is run as an applet

### Out of Scope

- The lighter `fsOpenFileSystemWithId` route (Option B) — rejected in favor of the proven exelix mechanism (Option A)
- Distributing any extracted Nintendo `.szs` assets — copyrighted; extraction is always on the user's own console
- Supporting `prod.keys`-on-SD extraction (the PC/offline-dump path) — only the on-device keyless-to-user path
- Changing the existing Phase B apply/remove flow — this milestone only *feeds* it base layouts
- Raw-NCA dump feature — not needed; we only need the `.szs` layouts

## Context

- Direct continuation of Phase B (native theme apply), which chose "Path B": read pre-extracted base layouts from `/themes/systemData/`, deferring on-device extraction. This milestone implements that extraction.
- Research (`.planning/research/EXTRACTION.md`) verified the real exelix mechanism against source @ `2618b0c`. Critical correction vs initial assumption: it is **not** a keyless FS-service mount; it mounts raw BIS, resolves via `lr`, derives the header key via SPL from public sources, and decrypts with a bundled hactool + mbedtls. "Keyless" = no user-provided `prod.keys`, not key-free.
- The hardest unknowns are hardware-only: whether the pinned public key sources still derive a valid header key on the target firmware, and the exact title-takeover launch/permission path.
- exelix code is GPL-2.0; thomaz is already GPLv2 (relicensed in Phase B), so the port is legally clean.

## Constraints

- **Verification**: Core/parsing logic gets host doctest coverage where feasible; the privileged extraction path is **hardware-only** and cannot be host-tested. Build via CI/Docker (devkitA64). Desktop build stays green (extraction is a Switch-only no-op on desktop).
- **License**: GPLv2 preserved; vendored hactool/mbedtls headers retained with attribution in `THIRD_PARTY.md`.
- **Architecture**: keep the `core/` (pure, testable) vs `platform/` (switch/fake) split. Extraction is a `platform/themes/*_switch.cpp` concern with a `*_fake.cpp` no-op for desktop.
- **No asset distribution**: never bundle or upload extracted `.szs`.
- **Title takeover**: the feature requires running as an Application; applet-mode must fail gracefully with guidance, not crash.

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Option A (port exelix BIS+SPL+hactool) over Option B (`fsOpenFileSystemWithId`) | Proven, in-tree precedent; Option B is unverified theory | — Pending |
| Keyless-to-user only (SPL-derived key); no `prod.keys` path | Best UX; avoids handling user key files | — Pending |
| Re-vendor hactool + mbedtls (reverse the Phase B exclusion) | Required by Option A | — Pending |
| Extraction is hardware-only verified; host tests cover only pure parsing | Privileged services can't run on host/desktop | — Pending |
| Requires title takeover; applet mode fails gracefully | Privileged FS/SPL access only granted to Applications | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-06-04 — milestone v0.4 (Extração de Temas) started*
