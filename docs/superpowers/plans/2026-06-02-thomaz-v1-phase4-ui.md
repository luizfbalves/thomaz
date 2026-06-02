# thomaz v1 — Phase 4: Bento UI (home + game list) — Design Contract & Plan

> Implemented via subagent against xfangfang/borealis demo patterns; verified by CI compile + user visual feedback (the UI cannot be rendered in CI or the sandbox).

**Goal:** A distinctive dark, modern UI: a **bento home** (Trapaças card + disabled "Em breve" cards) and a **game list** screen that lists installed games via the existing `ITitleService`. Tapping a game shows a "coming soon" placeholder (the cheat detail/toggles arrive after Phase 3b download lands).

**Visual identity (locked):**
- Theme: force **dark**. Background `#14151A`. Card surface `#1E2027`. Accent **`#7C5CFF`** (violet/indigo). Primary text `#FFFFFF`, secondary `#9AA0AA`, disabled `#52555E`.
- Feel: rounded cards (cornerRadius ~12), generous spacing, accent used for the primary action and focus highlight. Reference: Linear/Discord dark.

**Build/verify reality:** No rendering in CI/sandbox. Verification = (1) Switch CI build green, (2) optional desktop build the user runs to *see* it, (3) user screenshots → iterate. A **desktop FakeTitleService** returns sample games so the list is visible on PC without hardware.

---

## File Structure

```
source/
├── app/
│   ├── theme.hpp / theme.cpp          # registerThomazTheme(): colors + dark variant
│   ├── home_activity.hpp              # (moved from source/) bento home, CONTENT_FROM_XML_RES("activity/home.xml")
│   ├── home_activity.cpp              # click handler: Trapaças card -> push GameListActivity
│   ├── game_list_activity.hpp/.cpp    # lists ITitleService results into a Box/RecyclerFrame
├── platform/
│   ├── title_service_switch.*         # (exists) libnx impl
│   └── title_service_fake.hpp/.cpp    # NEW desktop-only fake (sample games) — guarded !__SWITCH__
└── main.cpp                           # MODIFIED: register theme; pick title service per platform; push HomeActivity
resources/
├── xml/activity/home.xml              # MODIFIED: bento layout (accent Trapaças card + 3 disabled cards)
├── xml/activity/game_list.xml         # NEW: app frame + a Box container the activity fills
└── i18n/{en-US,pt-BR}/thomaz.json     # MODIFIED: add game-list strings (title, empty state, coming-soon)
```

---

## Screens

### Home (bento) — `home.xml` + `HomeActivity`
- Root: `brls:AppletFrame` (title "thomaz") → a padded `brls:Box` (axis column).
- Greeting `brls:Label` (`@i18n/thomaz/home/greeting`), secondary color.
- Bento grid: a row `brls:Box` (axis row, wrap) containing:
  - **Trapaças card** — a focusable `brls:Box` (id `trapacasCard`), accent background `#7C5CFF`, cornerRadius 12, ~360x180, with an icon/emoji label + title `@i18n/thomaz/module/cheats/title` + subtitle. Registers a click action → `pushActivity(new GameListActivity())`.
  - **3 "Em breve" cards** — `brls:Box`, surface `#1E2027`, dimmed text `@i18n/thomaz/common/coming_soon`, **not focusable** (disabled look).
- HomeActivity: in `onContentAvailable()`, `getView("trapacasCard")` and `registerClickAction(...)` (or `getView<brls::Box>` + `addGestureRecognizer`/`registerClickAction`) to navigate. Follow the demo's pattern for clickable boxes.

### Game list — `game_list.xml` + `GameListActivity`
- Root: `brls:AppletFrame` (title `@i18n/thomaz/games/title`) → a scrolling container (`brls:ScrollingFrame` or `brls:RecyclerFrame`; a simple `brls:ScrollingFrame` + `brls:Box` column is fine for v1).
- Constructed with an `ITitleService*`. In `onContentAvailable()`: call `listInstalled()`, then for each `InstalledTitle` add a row — a `brls::ListItem`/`brls::DetailCell` (or a focusable Box) with the game **name** and the **version** as the sub-label. Tapping a row → `brls::Application::notify("@i18n/thomaz/games/coming_soon")` (toast) for now.
- **Empty state:** if no titles, show a centered `brls:Label` with `@i18n/thomaz/games/empty`.

---

## Theme (`theme.cpp`)
```
registerThomazTheme():
  brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);
  auto& dark = brls::Theme::getDarkTheme();
  dark.addColor("thomaz/bg",        nvgRGB(0x14,0x15,0x1A));
  dark.addColor("thomaz/surface",   nvgRGB(0x1E,0x20,0x27));
  dark.addColor("thomaz/accent",    nvgRGB(0x7C,0x5C,0xFF));
  dark.addColor("thomaz/text",      nvgRGB(0xFF,0xFF,0xFF));
  dark.addColor("thomaz/text_dim",  nvgRGB(0x9A,0xA0,0xAA));
  // override borealis background if the API allows (e.g. addColor("brls/background", ...))
```
Reference colors in XML as `@theme/thomaz/accent`, etc. Call `registerThomazTheme()` in `main()` after `Application::init()`.

---

## Platform selection (main.cpp)
```
#ifdef __SWITCH__
  auto svc = std::make_unique<thomaz::NsTitleService>(); svc->init();
#else
  auto svc = std::make_unique<thomaz::FakeTitleService>(); // sample games for PC
#endif
```
Pass `svc.get()` (an `ITitleService*`) to `GameListActivity` when the Trapaças card is tapped. Keep the service owned by main/Application for the app lifetime. (Simplest: a small singleton/global accessor, or construct the service inside GameListActivity per platform via a factory `make_title_service()`.)

`FakeTitleService::listInstalled()` returns ~5 believable sample titles, e.g.:
`{0x0100000000010000, "Super Mario Odyssey", "Nintendo", 393216}`, `{0x01006A800016E000, "Super Smash Bros. Ultimate", "Nintendo", 1966080}`, `{0x01007EF00011E000, "The Legend of Zelda: BotW", "Nintendo", 0}`, plus two more.

---

## i18n additions (both locales)
```
"games": { "title": "Meus Jogos" / "My Games",
           "empty": "Nenhum jogo encontrado — rode no Switch para ver seus jogos."
                  / "No games found — run on a Switch to see your games.",
           "coming_soon": "Cheats: em breve" / "Cheats: coming soon" }
```

---

## Implementation guidance (for the implementer)
- Ground EVERY Borealis API call in the upstream demo: read `lib/borealis/demo/src/*` (esp. an activity that has clickable cards and a list) and `lib/borealis/library/include/borealis/...` for exact view classes, XML attribute names, `getView`, `registerClickAction`, `notify`, theme color API, and how `@theme/...` is referenced in XML. Do NOT invent attributes.
- The Switch build is the source of truth: after each meaningful change, push and run the `build` workflow (`gh run watch`), read `--log-failed`, fix, repeat until the `thomaz.nro` artifact builds green.
- Keep `source/core` untouched (pure logic). UI lives in `source/app`. libnx stays in `source/platform`.
- Host tests (`make -C tests test`) must remain green (UI changes shouldn't touch them).

---

## Phase 4 done = definition
- Switch CI build GREEN with the bento home + game-list screens compiled in.
- Dark theme + violet accent applied.
- Home shows the Trapaças card (navigates to the game list) + disabled "Em breve" cards.
- Game list renders installed titles (real on Switch; sample via FakeTitleService on desktop), with an empty state and a "coming soon" tap.
- Host suite still green.

## Out of scope (later)
- Cheat detail screen with real toggles (needs Phase 3b download).
- Game icons (NACP icon extraction) — can be added once the list layout is approved.
- Per-game state badges (needs cheat data).
