# Save Backup & Restore — Design (Phase 1)

**Date:** 2026-06-03
**Status:** Approved for planning
**Scope:** thomaz Switch homebrew — local save backup/restore module

## Problem

Atmosphère users don't have real Nintendo accounts, so a console format wipes
their game saves with no cloud safety net. thomaz already lists installed titles
and manages cheats; this feature adds a Save Manager that reads real game saves
off the console, writes versioned backups to the SD card, and restores them
back into a game's save data.

## Scope (Phase 1)

**In:**
- Read real save data from the console for every installed game.
- Back up **all profiles** that have save data for a given game (automatic, no
  picker).
- Write versioned backups as a **folder tree** on the SD card (no new
  dependencies — no zip lib).
- Restore a chosen backup folder back into a game's save (with confirmation).
- Show last-backup date per game in the UI.

**Out (deferred to the cloud phase):**
- Any cloud upload/download. "Upload" in Phase 1 means "write to SD".
- App account / login / session. Login without a backend does nothing useful,
  so it waits for the real API.
- Single `.zip` archive packaging (would require adding minizip/miniz).

## Why these choices

- **Folder tree, not zip:** zero new build dependencies; easy to inspect; the
  whole folder uploads cleanly when the cloud phase arrives.
- **All profiles automatically:** Switch saves are per local profile (UID).
  Mounting succeeds only when a profile has data for that game, so enumerating +
  attempting a mount auto-discovers which profiles to include — no selector UI.
- **Backup + restore both in Phase 1:** SD survives a console format, so local
  restore-from-SD is genuinely useful even before cloud exists.
- **No login in Phase 1:** honest — a login screen with fake auth is throwaway
  work that will likely change when the real API lands.

## Architecture

Follows the existing thomaz pattern: **platform interface + Switch/fake impls +
pure `core/` logic + thin activity**. Mirrors `ITitleService` / `IHttpClient`.

### Save extraction (the technical core)

On Switch, a game's save is bound to a user profile (UID); a console always has
local profiles (created in System Settings) even without a Nintendo account.

Backup of one game:
1. Enumerate local profiles (`accountListAllUsers`).
2. For each (game, profile) pair, attempt to mount the save
   (`fsdevMountSaveData`). A successful mount means that profile has data for
   that game — this is how we auto-discover the profiles to include.
3. Recursively copy the mounted save's files to a versioned folder on SD,
   separated per profile.
4. Unmount.

Restore of one backup (destructive — always behind a confirmation prompt):
1. For each profile folder present in the backup, mount that game's save for
   that profile.
2. Clear the current save contents, copy the backup files back in.
3. `fsdevCommitDevice` — without the commit the Switch discards the writes.
4. If a profile in the backup no longer exists on the console, skip it and
   report which profiles were restored.

### SD layout

```
/switch/thomaz/saves/<titleId>/<timestamp>/
    manifest.json          (game name, titleId, date, profiles included)
    <profileUid>/          (that profile's save files; folder = UID, stable)
    ...
```

Profile folders are named by **UID** (stable across reboots/renames) so restore
can map a folder back to the right profile. The friendly profile name is stored
in `manifest.json` for display only.

The `manifest.json` makes each backup self-describing — needed for restore and
for the future cloud upload. The folder structure itself is the source of truth
for "last backup date" (most recent timestamp dir); no separate metadata DB.

### Interface — `source/platform/save_service.hpp`

```cpp
struct SaveProfile { std::uint64_t uid; std::string name; };
struct BackupEntry {
    std::string path;
    std::string timestamp;
    std::vector<std::string> profiles;
};

class ISaveService {
  public:
    virtual ~ISaveService() = default;
    // Profiles that currently have save data for this title.
    virtual std::vector<SaveProfile> profilesWithSave(std::uint64_t titleId) = 0;
    // Back up all profiles' saves for this title to the SD. False + *outError on failure.
    virtual bool backup(const InstalledTitle& title, std::string* outError) = 0;
    // Restore a backup folder back into the title's save. False + *outError on failure.
    virtual bool restore(const BackupEntry& entry, std::uint64_t titleId, std::string* outError) = 0;
};
```

- **Switch impl** (`save_service_switch.cpp`, guarded by `#ifdef __SWITCH__`):
  mount / copy / commit via libnx (`accountListAllUsers`, `fsdevMountSaveData`,
  `fsdevCommitDevice`).
- **Fake impl** (`save_service_fake.cpp`): creates/reads dummy folders in a
  simulated SD path so the full UI flow runs on desktop without a console.

### Pure core — `source/core/backup_store.{hpp,cpp}`

Testable, no libnx:
- Build SD paths for a title / timestamp.
- Scan the SD for a title's backup history (list of `BackupEntry`).
- Determine the last-backup date (most recent timestamp) or "never".
- Read/write `manifest.json`.

## UI / Flow

The home tile "Save Manager" (amber, `tile_saves`) stops being "coming soon" and
opens `SaveManagerActivity`.

### Screen 1 — Save Manager (game list)

Reuses `listInstalled()` (icon + name, same row layout as the other lists). Each
row shows:
- icon + game name
- subtitle with **last backup date** read from the SD (e.g. "Último backup:
  03/06 14:20") or a grey "Nunca" badge when no backup exists.

Footer hint: action to back up (e.g. "A: fazer backup" — wired via
`registerAction`).

### Screen 2 — Game detail (on row tap)

- game name + last backup date
- **"Fazer backup agora"** button
- list of existing backups for that game (timestamp folders) = history
- each history entry has a **"Restaurar"** action

Backup runs on a `brls::async` worker thread with a spinner (same pattern as the
cheats-db index load); on completion it refreshes the date and shows a
success/error toast.

Restore is destructive → always confirms first, then runs on a worker with a
spinner, then reports which profiles were restored.

## Error handling (real failure points)

- Game with no save in any profile → backup reports "nada pra salvar"; does not
  create an empty folder.
- Mount failure / SD full / copy failure → abort that backup, delete the partial
  folder (no leftover junk), show an error toast.
- Restore where a backup profile no longer exists on the console → skip that
  profile, report which profiles were restored.
- Every save write is behind a confirmation; the `fsdevCommitDevice` runs only
  after the full copy succeeds.

## Testing

- `core/backup_store` gets unit tests: path building, manifest parse, history
  ordering by date, "never" case.
- libnx mount/copy/commit logic is isolated in the Switch impl (not testable off
  the console).
- The fake impl exercises the full UI flow on desktop.

## Files

New:
- `source/platform/save_service.hpp` — `ISaveService` + structs
- `source/platform/save_service_switch.cpp` — libnx impl (`#ifdef __SWITCH__`)
- `source/platform/save_service_fake.cpp` — desktop impl
- `source/core/backup_store.{hpp,cpp}` — pure path/history/manifest logic
- `source/app/save_manager_activity.{hpp,cpp}` — Screen 1
- `source/app/save_detail_activity.{hpp,cpp}` — Screen 2
- `resources/xml/activity/save_manager.xml`, `save_detail.xml`
- i18n keys under `thomaz/saves/*` for all 5 languages
- tests for `core/backup_store`

Changed:
- `resources/xml/activity/home.xml` — Save Manager tile becomes focusable/active
- `source/app/home_activity.cpp` — wire the tile to push `SaveManagerActivity`
- `source/main.cpp` — construct `ISaveService` (Switch or fake) and pass it down

## Future (cloud phase, not this spec)

App account + login + session, real API upload/download, restore-from-cloud
after a format. The folder-tree backups and `manifest.json` are designed to
upload cleanly when that arrives.
