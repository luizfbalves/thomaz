---
status: testing
phase: 04-c-activity-hardening
source: [04-VERIFICATION.md]
started: 2026-06-05T20:00:00Z
updated: 2026-06-05T20:00:00Z
---

## Current Test

number: 1
name: Pop settings mid-update-check or mid-download — no crash, no stale network-error toast
expected: |
  Settings activity is destroyed cleanly; any in-flight update-check GET or installUpdate
  download_file call aborts without crashing or surfacing an error toast.
awaiting: user response

## Tests

### 1. Pop settings mid-update-check or mid-download, confirm no crash and no stale toast about network error
expected: Settings activity is destroyed cleanly; any in-flight update-check GET or installUpdate download_file call aborts without crashing or surfacing an error toast (WR-01: these specific transfers do not set req.cancelled, so they run to completion in the pool thread after teardown — leak, not crash; the alive guard drops the UI continuation so no toast fires).
result: [pending]

### 2. Pop settings while the update-confirm dialog is still open, then tap 'Yes'
expected: The dialog button's [this, rel, status] closure is called on a freed activity — either no crash (Borealis dismisses the dialog with the activity), or a UAF crash is observed (CR-01 site: settings_activity.cpp:172-173, raw this + raw status captured, no alive guard).
result: [pending]

### 3. Pop clear_cheats while the confirm-clear dialog is open, then tap the confirm button
expected: Either safe (dialog dismissed with activity) or UAF crash on accessing this->selections (CR-01 site: clear_cheats_activity.cpp:129-138, bare this captured, no alive guard).
result: [pending]

### 4. Pop mod_manager while the uninstall-confirm dialog is open, then tap confirm
expected: Either safe (dialog dismissed) or crash on this->refreshList() (CR-01 site: mod_manager_activity.cpp:192-197, [this, tid, modName] captured, no alive guard).
result: [pending]

### 5. Pop theme_detail while a download is starting — in the one-frame window after the download button fires brls::sync but before startDownload runs
expected: Either safe (alive guard in runAsync drops startDownload) or crash if the brls::sync fires before runAsync checks alive (CR-01 site: theme_detail_activity.cpp:98-105, brls::sync([this]{...startDownload()...}) with no alive capture; the one-frame deferral is the exposure window).
result: [pending]

## Summary

total: 5
passed: 0
issues: 0
pending: 5
skipped: 0
blocked: 0

## Gaps
