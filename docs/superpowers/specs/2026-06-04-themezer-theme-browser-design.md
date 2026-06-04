# Themezer theme browser — Phase A design

**Date:** 2026-06-04
**Status:** Approved (design)
**Scope:** Phase A of a larger "themes" milestone. Phase A = **discover + download**
`.nxtheme` files from [Themezer](https://themezer.net/switch). Native install
(`.nxtheme → .szs` via LayeredFS) is **Phase B** and explicitly out of scope here.

## Background

Themezer hosts custom Nintendo Switch home-menu themes. Two content types:

- **Themes** — each targets one part of the home menu (target/section): home
  (ResidentMenu), lock screen (Entrance), all-apps (Flaunch), settings (Set),
  player select (Psl), MyPage, News, Notification. One `.nxtheme` file.
- **Packs** — a curated group of themes covering several sections at once. A pack
  download yields several `.nxtheme` files.

Installing a theme on a console is a separate, heavy concern (convert the
differential `.nxtheme` to a firmware SZS, rebuild BNTX textures, patch BFLYT
layouts, write to `/atmosphere/contents/0100000000001000/romfs/lyt/`). That is
**Phase B** — a milestone-sized port of the NXThemes Installer engine. Phase A
deliberately stops at downloading the `.nxtheme` to the SD card so it delivers
value on its own and de-risks the bigger effort.

Themezer exposes a single GraphQL endpoint: `POST https://api.themezer.net/graphql`
(Apollo Server).

## Goals

- A new **"Temas"** entry on the home rail opening a Themezer browser.
- Browse a feed (default: most-downloaded), free-text search, and filter by home-menu
  section (target).
- Show results as a **thumbnail grid** (themes are visual).
- Open a detail view (larger preview; for a pack, the list of parts it will save).
- **Download** a theme or pack as `.nxtheme` file(s) into `sd:/themes/`.

## Non-goals (Phase A)

- Converting `.nxtheme` to SZS; applying/removing a theme; "active theme" state;
  extracting console firmware. All Phase B.
- Uploading/favoriting/login on Themezer.
- Live download progress bars (mirrors the current mods behavior — a future refinement).

## Approach

Mirror the proven **mods** subsystem (chosen over a single fat activity or
generalizing the mod browser, which would couple two genuinely different flows —
themes have no game association, have packs, and render as a grid).

### Architecture (core / platform / app)

**`core/themes/` — pure, unit-tested (no libnx, no IO; takes an injected fetcher)**

- `themezer_query.{hpp,cpp}`
  - Builds GraphQL request bodies (JSON POST) for:
    - feed (`packList` / `themeList`, ordered by downloads, paginated)
    - search (free-text `query`)
    - section filter (`target`)
    - detail (single pack/theme with `downloadUrl` + preview fields)
  - Parses responses into structs.
- Data model:
  ```cpp
  enum class ThemeKind { Theme, Pack };
  struct ThemeEntry {
      ThemeKind   kind;
      std::string id;            // Themezer id
      std::string name;
      std::string author;
      std::string target;        // section, e.g. "ResidentMenu" (empty for packs)
      std::string preview_url;   // CDN image for the grid/detail
      std::uint64_t downloads = 0;
  };
  struct ThemeDownload {         // resolved at detail time
      std::string filename;      // "<name>.nxtheme"
      std::string url;           // direct download
  };
  struct ThemeDetail {
      ThemeEntry entry;
      std::vector<ThemeDownload> files; // 1 for a theme, N for a pack
  };
  struct BrowsePage {
      std::vector<ThemeEntry> entries;
      int  page = 1;
      bool is_complete = true;   // no further pages
  };
  ```
- Result/status enums mirror `mod_browse` (`Ok` / `NetworkError` / `NotFound`).
- A `UrlFetcher`-style callback (same pattern as mods) performs the actual POST,
  so parsing is testable with fixtures.

**`platform/themes/` — IO, reuses existing primitives**

- `theme_download.{hpp,cpp}`
  - `download_theme(const ThemeDetail&, ...)`: for each `ThemeDownload`, fetch the
    bytes (reuse `download_file`) and write to
    `sd:/themes/<Author> - <Name>/<filename>.nxtheme` (NXThemes Installer
    convention). A pack writes all its files into the one folder.
  - On any file failure, remove the partial folder and report the error (matches
    `import_archive` discipline).
  - `theme_already_downloaded(const ThemeEntry&)`: true if the target folder
    already exists (drives the "já baixado" badge).
- `themes_root()`: `sd:/themes` on Switch, `themes` (working dir) on desktop.

**`app/` — activities**

- `theme_browser_activity` — the grid.
  - Header: **[Buscar]** (opens IME) and **[Parte ▾]** (section filter:
    início/bloqueio/todos-apps/config/…; "todas" = no filter).
  - Default view: most-downloaded feed (page 1).
  - Grid of cards: preview image + name + author + ⬇downloads. A "Carregar mais"
    row appends the next page when not complete (mirrors the mods load-more).
  - Thumbnails: each card downloads its `preview_url` on a worker thread and
    renders via `setImageFromMem`; a neutral placeholder shows until it loads;
    cache previews in memory for the session (keyed by URL).
  - Tapping a card pushes the detail activity.
- `theme_detail_activity` — larger preview; for a pack, the list of parts that
  will be saved; **Baixar** button → `download_theme`, then a notification.
  Until Phase B, a short note explains the file lands in `sd:/themes/` for the
  NXThemes Installer.

### UI placement

Add a **"Temas"** card to the home rail next to settings/saves/mods, following
the existing rail card pattern (title Label, rounded, focusable). i18n keys under
a new `themes` namespace (`resources/i18n/<locale>/themes.json`), pt-BR + en-US.

## Data flow

1. `onContentAvailable` → spinner; worker POSTs the feed query via `IHttpClient`;
   `brls::sync` back to build the grid (same async discipline as mods).
2. Each visible card kicks an async preview fetch → `setImageFromMem` on the UI thread.
3. Search/filter/load-more re-issue the query with updated `query`/`target`/`page`.
4. Detail tap → worker resolves the detail (download URLs) → build the view.
5. Download tap → worker `download_theme` → notify success/failure; mark as
   downloaded.

## Error handling

- Transport failure (status 0) → "verifique a conexão" (same wording as mods).
- A preview image that fails to load stays on the placeholder; never breaks the grid.
- A failed download notifies the user and leaves **no** half-written folder.
- Empty feed/search → an empty-state label (mirrors `mods/no_results`).

## Testing

- **Core (`core/themes/`)**: doctest cases (like `test_mod_browse`) covering
  query/body construction, feed parsing, search parsing, detail/pack parsing
  (1 file vs N files), and pagination/`is_complete`, all from JSON fixtures.
- **Platform/UI/Switch**: validated by the desktop build + boot-smoke and the
  Switch `.nro` CI, as in prior work (binary/libnx paths aren't sandbox-testable).
- Desktop fake: a small canned fetcher so the grid renders during desktop
  iteration without hitting the network (mirrors the fake services already used).

## Open questions / decisions made

- **Downloaded badge:** included in v1 (cheap: directory existence check).
- **Preview cache:** in-memory per session only (no SD cache in Phase A).
- **GraphQL field names:** the exact `packList`/`themeList` field/argument names
  will be pinned against the live schema (introspection) when implementing the
  query builder; the design does not depend on their spelling.

## Phase B (future, for context only)

Native install: vendor/port the NXThemes Installer C++ core
(`SwitchThemesCommonNX`) — NXTheme container, SARC, Yaz0, BNTX, BFLYT — apply the
differential onto base layout files, write the SZS to LayeredFS, manage the
active theme, and prompt for reboot. Separate spec → plan → implementation.
