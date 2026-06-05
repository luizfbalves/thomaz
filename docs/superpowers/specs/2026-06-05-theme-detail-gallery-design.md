# Theme Detail Gallery — Design

**Date:** 2026-06-05
**Status:** Approved (design)

## Problem

The theme detail screen shows a single image (`preview_url`, the small JPEG
thumbnail). The Themezer GraphQL API exposes far more per theme — a multi-size
rendered preview, the full-resolution background/wallpaper, and ~11 UI icons —
and per pack, every member theme with its own preview. None of that reaches the
user. The limitation is entirely on our side: query, data model, parser, and UI
are all hard-wired to one image.

## Goal

Turn the detail screen into a proper gallery: a large hero image with a
horizontal, D-pad-navigable thumbnail strip, plus a fullscreen viewer. Show a
centered loading state while the detail request is in flight and reveal the
whole screen at once when it resolves.

## What the API exposes (confirmed via introspection)

Endpoint: `https://api.themezer.net/graphql`. Real type names are `SwitchTheme`
and `SwitchPack` (not `Theme`/`Pack`).

- **`SwitchTheme.screenshotPreview`** (`ImageSizesHd`): `tinyUrl`, `thumbUrl`,
  `sdUrl`, `hdUrl`, `jpgThumbUrl`, `jpgHdUrl` — one rendered preview, many sizes.
- **`SwitchTheme.assets`** (`SwitchThemeAssets`): `backgroundImageUrl` plus
  per-component icon URLs — `albumIconUrl`, `homeIconUrl`, `newsIconUrl`,
  `shopIconUrl`, `controllerIconUrl`, `settingsIconUrl`, `powerIconUrl`,
  `nsoIconUrl`, `cardIconUrl`, `shareIconUrl`. Icons the theme does not override
  come back null.
- **`SwitchPack.themes`**: array of member `SwitchTheme`s, each with its own
  `screenshotPreview` + `assets`.
- **`SwitchPack.collagePreview`** (`ImageSizesFhd`): the pack collage (existing).

## Decisions

- **Single theme gallery:** Preview + Background + Icons (full set).
- **Pack gallery:** one thumbnail per member theme (its preview); swapping
  cycles through the pack's themes.
- **Fullscreen:** focusing a thumbnail and pressing **A** opens a fullscreen
  viewer; **B** closes.
- **Layout:** hero image (left column, existing slot) + horizontal thumbnail
  strip below it; identity + description + download stay in the right column.
- **Loading:** centered spinner only while fetching; reveal all content on
  resolve; centered error message on network failure.

## Design

### Data model (`source/core/themes/themezer_types.hpp`)

```cpp
struct GalleryImage {
    std::string url;        // hd-resolution image (hero + fullscreen)
    std::string thumb_url;  // small image for the strip
    std::string label;      // "Preview", "Background", icon name, or member name
};
```

Add to `ThemeDetail`:

```cpp
std::vector<GalleryImage> gallery;
```

`ThemeEntry::preview_url` is unchanged — the browse grid still uses it. The
gallery is purely additive.

### API queries (`source/core/themes/themezer_query.cpp`)

- **Theme detail** — extend the selection with
  `screenshotPreview{ hdUrl thumbUrl }` and an `assets{ ... }` block listing
  `backgroundImageUrl` and all icon URLs above.
- **Pack detail** — extend with
  `themes{ name screenshotPreview{ hdUrl thumbUrl } }`.

### Parser (`source/core/themes/themezer_json.cpp`)

Build `ThemeDetail::gallery`:

- **Theme:** `[Preview, Background, <each non-null icon>]`. Skip null/empty
  URLs. Each `GalleryImage` carries `hdUrl` as `url` and `thumbUrl` as
  `thumb_url` (icons may only have one URL — use it for both). Labels:
  "Preview", "Background", and a friendly icon name (e.g. "Home", "Album").
- **Pack:** one `GalleryImage` per member theme, `label` = member name (falls
  back to target/empty), using the member's `screenshotPreview`.
- Malformed/missing `assets` yields a valid, possibly preview-only gallery.

### UI layout (`resources/xml/activity/theme_detail.xml`)

- Keep `detailPreview` (460×259 `Image`) as the **hero**, bound to the
  currently-selected gallery item.
- Add a horizontal **thumbnail strip** below the hero: a `brls::ScrollingFrame`
  wrapping a `row` `Box`. Thumbnails are built programmatically (same pattern as
  the browse grid's card construction in `theme_browser_activity.cpp`).
- Wrap the whole two-column content `Box` so it can be hidden during load;
  centered spinner + centered error label live at the frame level.

### Interaction (`source/app/theme_detail_activity.cpp`)

- **Loading:** in `onContentAvailable`, hide the content `Box`, show a centered
  `ProgressSpinner`, and kick off the detail fetch. Remove today's eager
  `entry.preview_url` fetch — we wait for the resolve (which carries the HD
  preview).
- **On resolve (success):** hide spinner, reveal content, build the thumbnail
  strip, select the first gallery item (loads its hero), and populate
  name/author/description/parts as today.
- **On resolve (error):** hide spinner, show a centered error message; do not
  reveal empty content.
- **Thumbnail focus** → swap the hero to that item. Hero loads `url` (HD) on
  demand, cached by URL so revisits are instant. Thumbnails load `thumb_url`.
- **Press A on a thumbnail** → open the fullscreen viewer.
- All async image loads keep the existing `alive` guard pattern.

### Fullscreen viewer (`source/app/image_viewer_activity.{hpp,cpp}` — new)

A minimal activity pushed via `brls::Application::pushActivity`: the image
filling the screen on a dim backdrop, the gallery item's `label` as a caption,
**B** to close. Reuses the hero's already-decoded HD bytes when available to
avoid a refetch; otherwise loads `url` async.

## Edge cases

- **Empty gallery** (API returned nothing usable) → fall back to today's single
  `preview_url` behavior so the screen is never blank.
- **Failed thumb/hero load** → leave the placeholder, do not crash.
- **Pack with one member** → strip shows that single thumbnail.
- **Many icons** (up to ~13 items) → the strip scrolls horizontally; focusing an
  off-screen thumb auto-scrolls it into view.

## Testing

- **Unit (parser, `themezer_json`):**
  - Theme response → gallery is `[Preview, Background, icons…]` with null icons
    skipped and correct labels.
  - Pack response → one gallery item per member with member-name labels.
  - Missing/partial `assets` → valid gallery (preview-only if needed).
- **Manual on-device:** loading state, hero swap on D-pad, horizontal scroll
  with a many-icon theme, fullscreen open/close, pack member cycling, network
  error message — consistent with how the rest of the themes feature is
  validated.

## Out of scope

- Caching gallery images to disk across sessions.
- Animations/transitions beyond Borealis defaults.
- Editing or downloading individual assets (icons/background) separately.
