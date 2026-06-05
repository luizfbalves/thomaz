---
phase: 01-privileged-extraction-spike
plan: 05
subsystem: docs
tags: [takeover-02, spl-provenance, d-07, title-takeover, third-party, portuguese-doc]
dependency_graph:
  requires: [01-04-firmware-extract-entry-point]
  provides: [takeover-02-doc, spl-key-source-provenance]
  affects:
    - docs/title-takeover.md
    - THIRD_PARTY.md
tech_stack:
  added:
    - "docs/title-takeover.md — user-facing hold-R launch instructions in Portuguese (TAKEOVER-02)"
    - "THIRD_PARTY.md ## SPL key sources block — Atmosphère 1.7.1/b39e29d provenance + firmware pending marker (D-07 partial)"
  patterns:
    - "existing THIRD_PARTY.md attribution block shape (## name, Source, Pinned, Files, License)"
    - "RESEARCH Title-Takeover Doc Skeleton verbatim as base (PATTERNS.md Title-takeover doc)"
key_files:
  created:
    - docs/title-takeover.md
  modified:
    - THIRD_PARTY.md
decisions:
  - "Firmware version written as explicit Portuguese pending marker in both files, not a placeholder version number — hardware run has not occurred (critical_context)"
  - "Atmosphère 1.7.1 / commit b39e29d is the real pinned provenance (in key_loader_switch.cpp lines 17-23, 119)"
  - "override_config.ini (Assumption A4) documented as untested-until-hardware-run, not omitted"
  - "Doc prose in Portuguese per milestone language (Extração de Temas); technical identifiers kept verbatim"
metrics:
  duration_seconds: 71
  completed_date: "2026-06-05"
  tasks_completed: 1
  tasks_total: 2
  files_created: 1
  files_modified: 1
---

# Phase 01 Plan 05: Title-Takeover Doc + SPL Provenance Summary

**One-liner:** User-facing Portuguese hold-R title-takeover launch doc (TAKEOVER-02) + extended THIRD_PARTY.md SPL key-source block with Atmosphère 1.7.1/b39e29d provenance and explicit pending-hardware-run firmware marker (D-07 partial — firmware version awaits Task 2 human-verify).

## What Was Built

### Task 1: `docs/title-takeover.md` (TAKEOVER-02) + THIRD_PARTY.md extension

**`docs/title-takeover.md`** — written in Portuguese, under 120 lines, contains:

1. **Passos numerados (hold-R):** A partir do Home menu → SEGURAR R → abrir JOGO instalado (não Álbum/Galeria) → manter R durante o logotipo da Nintendo → hbloader abre em Modo Aplicação → abrir thomaz → rodar "Extrair layouts do firmware".

2. **Nota sobre modo Applet vs. Aplicação:** Modo Applet (lançado pela Galeria) falta permissões FS/SPL → thomaz exibe aviso de relançamento via title takeover e retorna sem chamar nenhum serviço privilegiado (TAKEOVER-01).

3. **Nota sobre `override_config.ini` (Suposição A4):** Documentado como não necessário no caso padrão; marcado como "não testado em hardware ainda — a ser confirmado na primeira execução" (per critical_context directive).

4. **Seção Proveniência (D-07):**
   - Atmosphère release **1.7.1**, commit **b39e29d** — fontes de chave SPL PÚBLICAS pinadas em `key_loader_switch.cpp` linhas 17–42.
   - Versão de firmware: **"Pendente — a registrar após a primeira execução em hardware (`setsysGetFirmwareVersion`)"** — nenhuma versão fabricada ou estimada.

**`THIRD_PARTY.md` `## SPL key sources (Atmosphère)` block** — expandido de placeholder para entrada completa:
- Release 1.7.1 + commit b39e29d explicitamente declarados.
- Referência ao arquivo in-tree (`key_loader_switch.cpp` linhas 17-42).
- Nota de licença (GPLv2-or-later do projeto Atmosphère; fontes públicas não são material de chave secreto).
- Versão de firmware: mesma marker pendente-hardware-run, com referência ao `docs/title-takeover.md`.

## OPEN — Task 2: Human-Verify (BLOCKING)

**Task 2 is a `type="checkpoint:human-verify"` gate. It was NOT executed by this agent.**

| Item | Status |
|------|--------|
| Task 1: Write doc + THIRD_PARTY.md | DONE — commit b7d3548 |
| Task 2: Human-verify doc steps + recorded provenance | **OPEN — awaiting hardware run** |

**What remains open:**
- The hardware spike (plan 04 firmware_extract_switch.cpp) has been code-completed but NOT yet run on a real Nintendo Switch console.
- The firmware version (`setsysGetFirmwareVersion` → `fw.major.minor.micro`) is unknown until the first hardware run.
- Once the hardware run completes: (a) record the firmware version in `docs/title-takeover.md` Proveniência table and in `THIRD_PARTY.md` `## SPL key sources` block; (b) confirm the Atmosphère 1.7.1/b39e29d key sources successfully derived a valid header key on that firmware; (c) confirm the hold-R steps are reproducible on the user's console.

**STATE note:** Phase 01 awaits on-hardware validation before TAKEOVER-02 firmware provenance (D-07 / Success Criterion #4) is final. The pending marker in both files is intentional and must be replaced with the real firmware version after the first hardware run.

**Resume signal for Task 2:** "approved" — or describe corrections (e.g. wrong firmware version, missing override_config note).

## Deviations from Plan

### Auto-decisions (no structural deviations)

**1. [Critical Context Override] Firmware version recorded as explicit pending marker, not a real version**

- **Found during:** Pre-execution context read (critical_context in prompt).
- **Issue:** The plan's acceptance criteria require the "real" firmware version from the hardware run. The hardware run has NOT been performed. Fabricating a version would violate the critical_context directive and introduce false provenance.
- **Fix:** Both `docs/title-takeover.md` and `THIRD_PARTY.md` carry the Portuguese pending marker "Pendente — a registrar após a primeira execução em hardware (`setsysGetFirmwareVersion`)" instead of a version number. This is the correct state pre-hardware-run.
- **Impact:** Task 2 checkpoint (human-verify) remains open and blocking — per the plan's design.

**2. [Auto-decision] Doc prose written in Portuguese**

- Per critical_context: "Write user-facing prose in PORTUGUESE (the milestone 'Extração de Temas' is Portuguese)". Technical identifiers (commit hashes, function names, firmware X.Y.Z format) kept verbatim in English.

## Known Stubs

| Stub | File | Line (approx.) | Reason |
|------|------|-----------------|--------|
| Firmware version pending marker | `docs/title-takeover.md` | Proveniência table | Hardware run has not occurred; will be replaced with `fw.major.minor.micro` after Task 2 verification |
| Firmware version pending marker | `THIRD_PARTY.md` | `## SPL key sources` block | Same — mirrors the doc pending marker |

These stubs are intentional and correctly represent the current state. They must be replaced after the on-hardware spike run (Task 2).

## Threat Surface Coverage

| Threat ID | Category | Status |
|-----------|----------|--------|
| T-01-19 | Repudiation (provenance) | Partially mitigated — Atmosphère 1.7.1/b39e29d is recorded; firmware version awaits hardware run (Task 2) |
| T-01-20 | Info Disclosure | Accept — only public info documented; no secret, no derived key, no `prod.keys` |
| T-01-SC | Tampering (installs) | N/A — docs-only plan, no packages installed |

## Threat Flags

No new threat surface introduced. This is a documentation-only plan.

## Self-Check: PASSED

Files:
- docs/title-takeover.md: FOUND
- THIRD_PARTY.md: modified (verified grep checks passed)

Commits:
- b7d3548: docs(01-05): add title-takeover user doc and extend SPL key-source provenance — FOUND

Verification (plan automated checks):
- test -f docs/title-takeover.md: PASSED
- grep -qi 'hold' docs/title-takeover.md: PASSED
- grep -Eqi 'applet|album' docs/title-takeover.md: PASSED
- grep -Eqi 'firmware|[0-9]+\.[0-9]+\.[0-9]+' docs/title-takeover.md: PASSED (1.7.1 in provenance)
- grep -qi 'atmos' docs/title-takeover.md: PASSED
- grep -qi 'atmos' THIRD_PARTY.md: PASSED
