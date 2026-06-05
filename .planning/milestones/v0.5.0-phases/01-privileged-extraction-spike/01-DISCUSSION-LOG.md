# Phase 1: Privileged Extraction Spike - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-04
**Phase:** 1-privileged-extraction-spike
**Areas discussed:** Code shape (spike framing)

---

## Gray Areas Presented (multi-select)

The user was offered four HOW decisions to lock down (code shape, spike output path,
title-takeover method, mbedtls + key-source vendoring). Instead of selecting from the
menu, the user challenged the framing via free text.

| Option | Description | Selected |
|--------|-------------|----------|
| Code shape: keeper vs throwaway | Production-shaped code Phase 2 grows into, or disposable spike harness | ✓ (resolved → keeper) |
| Spike output path | Canonical `/themes/systemData/` vs throwaway `/thomaz_spike/` | ✓ (resolved → canonical) |
| Title-takeover launch method | hbloader hold-`R` override vs forwarder NSP | (defaulted → hold-`R`) |
| mbedtls + key-source vendoring | build-from-source-in-CI vs prebuilt `.a`; which Atmosphère commit | (defaulted → build from source) |

**User's choice (free text):** "nao quero esse spike quero o codigo real de fato, o
spike eh um teste?" — then, after clarification: "se ja tem provas de que funciona em
outros projetos vamos seguir."

**Notes:**
- User questioned whether "spike" meant throwaway test code. Clarified that Phase 1 is a
  thin vertical slice of REAL code (the actual `key_loader`/`hactool` port), narrow in
  scope (one szs) only to validate hardware-only unknowns — not disposable. Phase 2
  extends it.
- User accepted proceeding directly to real code on the grounds that Option A (the
  exelix BIS+SPL+hactool mechanism) is already proven in production elsewhere. Clarified
  that the residual risk is thomaz-specific and hardware-only (target firmware key
  derivation + title-takeover permission set), which the one-szs slice still validates
  cheaply.
- Decision locked: **real/keeper code** → which implied **canonical output path**. The
  two unselected areas (title-takeover method, mbedtls/key-source vendoring) were
  resolved with stated sensible defaults, recorded in CONTEXT.md as D-04…D-07.

---

## Claude's Discretion

- Exact applet-mode message wording + surface (default: user-facing Borealis dialog).
- Internal file/function/class names for the ported `key_loader`/`hactool` code.
- Whether to port `RomfsCache` now (default: defer to Phase 2).

## Deferred Ideas

- Forwarder NSP → Phase 4 / TAKEOVER-03.
- Full extraction (all titles/all szs) → Phase 2.
- Theme UI action, `base_missing` unblock, state + messaging, firmware-version record → Phase 3.
