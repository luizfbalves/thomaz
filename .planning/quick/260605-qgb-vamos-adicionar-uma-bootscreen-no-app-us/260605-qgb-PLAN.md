---
phase: quick-260605-qgb
plan: 01
type: execute
wave: 1
depends_on: []
files_modified:
  - source/app/boot_activity.hpp
  - source/app/boot_activity.cpp
  - resources/xml/activity/boot.xml
  - resources/i18n/en-US/thomaz.json
  - resources/i18n/pt-BR/thomaz.json
  - resources/i18n/fr/thomaz.json
  - resources/i18n/ru/thomaz.json
  - resources/i18n/zh-Hans/thomaz.json
  - source/main.cpp
autonomous: true
requirements: [QGB-bootscreen]

must_haves:
  truths:
    - "When no session is saved, BootActivity shows before HomeActivity"
    - "When a session is already restored, BootActivity is skipped and HomeActivity opens directly"
    - "BootActivity has a black (#000000) background, app icon on the left, two buttons on the right"
    - "Pressing Entrar pushes AuthActivity; on successful auth it replaces with HomeActivity"
    - "Pressing Continuar sem login pushes HomeActivity directly"
    - "The app compiles cleanly (desktop: cmake -DUSE_SDL2=ON; Switch: devkitPro build)"
  artifacts:
    - path: "source/app/boot_activity.hpp"
      provides: "BootActivity class declaration"
    - path: "source/app/boot_activity.cpp"
      provides: "BootActivity implementation"
    - path: "resources/xml/activity/boot.xml"
      provides: "Boot screen layout (icon left, buttons right)"
  key_links:
    - from: "source/main.cpp"
      to: "BootActivity"
      via: "conditional push: if no session -> pushActivity(new BootActivity(...))"
    - from: "BootActivity (loginBtn click)"
      to: "AuthActivity"
      via: "brls::Application::pushActivity(new AuthActivity(feedClient.get(), onAuthed))"
    - from: "AuthActivity onAuthed callback"
      to: "HomeActivity"
      via: "brls::Application::popActivity + pushActivity(new HomeActivity(...))"
---

<objective>
Add a boot/splash screen (BootActivity) that shows before HomeActivity when no saved session exists.

Purpose: Give the user a clear entry point — login or continue as guest — with on-brand visual identity (app icon prominent on the left).

Output:
- source/app/boot_activity.hpp + .cpp
- resources/xml/activity/boot.xml
- i18n keys in all five locales
- main.cpp wired to conditionally push BootActivity instead of HomeActivity when restoredSession is empty

Design decisions (stated explicitly):
- Skip condition: BootActivity only shows when `!restoredSession.has_value()`. When a session is already restored, main.cpp pushes HomeActivity directly as before. This avoids forcing a re-login on every launch while still giving first-time/logged-out users a proper welcome.
- No new custom XML view registration in main.cpp is needed: BootActivity uses only standard brls views (brls:AppletFrame, brls:Box, brls:Image, brls:Label) plus the already-registered AnimatedBox.
- CMakeLists.txt does NOT need editing: `file(GLOB_RECURSE MAIN_SRC ...)` auto-picks up new .cpp files; XML resources are included via PROJECT_RESOURCES.
- Background: literal `#000000` (true black as specified). AppletFrame backgroundColor overrides the frame background; the inner Box also sets backgroundColor="#000000" so the content area matches.
- i18n: add new keys `thomaz/boot/login` and `thomaz/boot/guest` in all five locales rather than reusing auth keys, so the copy is independently adjustable.
</objective>

<execution_context>
@$HOME/.claude/gsd-core/workflows/execute-plan.md
@$HOME/.claude/gsd-core/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@source/main.cpp
@source/app/home_activity.hpp
@source/app/auth_activity.hpp
@source/app/thomaz_activity.hpp
@resources/xml/activity/auth.xml
@resources/xml/activity/home.xml
</context>

<tasks>

<task type="auto">
  <name>Task 1: Create BootActivity (header, implementation, XML layout)</name>
  <files>
    source/app/boot_activity.hpp
    source/app/boot_activity.cpp
    resources/xml/activity/boot.xml
  </files>
  <action>
Create three files.

**resources/xml/activity/boot.xml**

Root is `brls:AppletFrame` with `title=""` (empty title; no top bar text needed for a splash) and `backgroundColor="#000000"`. Inside it, a single `brls:Box` with `axis="row"`, `grow="1.0"`, `backgroundColor="#000000"`.

Left half: `brls:Box` with `axis="column"`, `grow="1.0"`, `justifyContent="center"`, `alignItems="center"`, `backgroundColor="#000000"`. Inside it, a `brls:Image` with `id="bootIcon"`, `image="@res/icon/icon.png"`, `width="180"`, `height="180"` (square icon, centered).

Right half: `brls:Box` with `axis="column"`, `grow="1.0"`, `justifyContent="center"`, `alignItems="center"`, `backgroundColor="#000000"`. Inside it, two `AnimatedBox` buttons:

Button 1 — login:
```
id="loginBtn"
entranceDelay="80"
axis="row" justifyContent="center" alignItems="center"
width="320" height="60"
marginBottom="18"
cornerRadius="16"
backgroundColor="@theme/thomaz/tile_cheats"
focusable="true" highlightCornerRadius="16"
hideHighlightBackground="true"
```
With a child `brls:Label id="loginLabel" text="@i18n/thomaz/boot/login" fontSize="19" textColor="#FFFFFF"`.

Button 2 — guest:
```
id="guestBtn"
entranceDelay="180"
axis="row" justifyContent="center" alignItems="center"
width="320" height="60"
cornerRadius="16"
backgroundColor="@theme/thomaz/surface_1"
borderColor="@theme/thomaz/line" borderThickness="1"
focusable="true" highlightCornerRadius="16"
hideHighlightBackground="true"
```
With a child `brls:Label id="guestLabel" text="@i18n/thomaz/boot/guest" fontSize="19" textColor="@theme/thomaz/text"`.

**source/app/boot_activity.hpp**

```
#pragma once
#include <functional>
#include <borealis.hpp>
#include "app/thomaz_activity.hpp"
#include "platform/http_client.hpp"
#include "platform/title.hpp"
#include "platform/save_service.hpp"
#include "platform/auth_client.hpp"
#include "platform/saves/cloud_save_client.hpp"

namespace thomaz {

class BootActivity : public ThomazActivity {
  public:
    BootActivity(ITitleService* titleService,
                 IHttpClient* http,
                 ISaveService* saveService,
                 IAuthClient* feed,
                 ICloudSaveClient* cloudSaves);

    CONTENT_FROM_XML_RES("activity/boot.xml");
    void onContentAvailable() override;

  private:
    void goHome();

    ITitleService* titleService;
    IHttpClient*   http;
    ISaveService*  saveService;
    IAuthClient*   feed;
    ICloudSaveClient* cloudSaves;
};

} // namespace thomaz
```

**source/app/boot_activity.cpp**

Include: `boot_activity.hpp`, `app/home_activity.hpp`, `app/auth_activity.hpp`, `<borealis.hpp>`, `<borealis/core/i18n.hpp>`.

Constructor: member-initialize all five service pointers.

`goHome()`: calls `brls::Application::pushActivity(new thomaz::HomeActivity(titleService, http, saveService, feed, cloudSaves))`.

`onContentAvailable()`:
1. Wire `loginBtn` click: `this->getView("loginBtn")->registerClickAction(...)`. The click handler pushes `AuthActivity`: `brls::Application::pushActivity(new thomaz::AuthActivity(this->feed, [this]() { this->goHome(); }))`. Also add `TapGestureRecognizer` on `loginBtn` for touch support.
2. Wire `guestBtn` click: calls `this->goHome()` directly. Also add `TapGestureRecognizer`.
3. Set initial focus on `loginBtn` via `brls::Application::giveFocus(this->getView("loginBtn"))`.

Note on `goHome()`: it uses `pushActivity`, not `popActivity` + `pushActivity`, because BootActivity should remain in the stack under HomeActivity momentarily — Borealis will pop it naturally when the user hits B from HomeActivity (acceptable: they'll just see the boot screen momentarily before the app exits via globalQuit). If that back-navigation is undesirable, use `brls::Application::popActivity(brls::TransitionAnimation::NONE, [this]() { this->goHome(); })` inside the click handler instead of the shared helper. Prefer the pop-then-push form to avoid stacking BootActivity under HomeActivity: in the loginBtn handler, after AuthActivity authed callback fires, `brls::Application::popActivity(brls::TransitionAnimation::NONE, [this](){ ... pushActivity(new HomeActivity(...)) })`. For guestBtn, same pattern: `brls::Application::popActivity(brls::TransitionAnimation::NONE, [this](){ ... pushActivity(new HomeActivity(...)) })`.

Implement `goHome()` as a private lambda-capture-safe helper that constructs HomeActivity with all five stored service pointers. Wire both paths (onAuthed callback and guestBtn click) through it.

No `brls::Application::registerXMLView` is needed — BootActivity itself is not a custom view; AnimatedBox is already registered in main.cpp before pushActivity is called.
  </action>
  <verify>
    <automated>cd /home/solid/www/personal/playground/thomas && cmake -B build-desktop -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -5 && cmake --build build-desktop --target thomaz 2>&1 | grep -E "error:|warning:|Built target"</automated>
  </verify>
  <done>boot_activity.hpp, boot_activity.cpp, and boot.xml all exist; desktop cmake build completes without errors referencing these three files.</done>
</task>

<task type="auto">
  <name>Task 2: Add i18n keys for boot screen in all five locales</name>
  <files>
    resources/i18n/en-US/thomaz.json
    resources/i18n/pt-BR/thomaz.json
    resources/i18n/fr/thomaz.json
    resources/i18n/ru/thomaz.json
    resources/i18n/zh-Hans/thomaz.json
  </files>
  <action>
Add a `"boot"` object inside the top-level JSON of each locale's `thomaz.json`. The object has two keys: `"login"` and `"guest"`.

Translations per locale:

- **en-US**: `"login": "Log in"`, `"guest": "Continue without login"`
- **pt-BR**: `"login": "Entrar"`, `"guest": "Continuar sem login"`
- **fr**: `"login": "Se connecter"`, `"guest": "Continuer sans connexion"`
- **ru**: `"login": "Войти"`, `"guest": "Продолжить без входа"`
- **zh-Hans**: `"login": "登录"`, `"guest": "不登录继续"`

Insert the `"boot"` key after the existing `"auth"` key in each file to keep grouping logical. Preserve all existing content; this is an additive edit only.
  </action>
  <verify>
    <automated>python3 -c "
import json, sys
locales = ['en-US','pt-BR','fr','ru','zh-Hans']
base = '/home/solid/www/personal/playground/thomas/resources/i18n'
for loc in locales:
    with open(f'{base}/{loc}/thomaz.json') as f:
        d = json.load(f)
    assert 'boot' in d['thomaz'], f'{loc}: missing thomaz.boot'
    assert 'login' in d['thomaz']['boot'], f'{loc}: missing boot.login'
    assert 'guest' in d['thomaz']['boot'], f'{loc}: missing boot.guest'
print('All i18n keys OK')
"</automated>
  </verify>
  <done>All five locale files parse as valid JSON and contain thomaz/boot/login and thomaz/boot/guest keys.</done>
</task>

<task type="auto">
  <name>Task 3: Wire BootActivity into main.cpp (conditional on missing session)</name>
  <files>
    source/main.cpp
  </files>
  <action>
In `source/main.cpp`:

1. Add include: `#include "app/boot_activity.hpp"` alongside the existing activity includes.

2. Replace the existing `brls::Application::pushActivity(new thomaz::HomeActivity(...))` call with a conditional:

```
if (restoredSession.has_value()) {
    // Session already restored — skip boot screen, go directly to home.
    brls::Application::pushActivity(
        new thomaz::HomeActivity(titleService.get(), httpClient.get(), saveService.get(),
                                 feedClient.get(), cloudSaveClient.get()));
} else {
    // No saved session — show boot screen (login or guest).
    brls::Application::pushActivity(
        new thomaz::BootActivity(titleService.get(), httpClient.get(), saveService.get(),
                                 feedClient.get(), cloudSaveClient.get()));
}
```

The `restoredSession` variable already exists at that point in the file (declared on line 95: `auto restoredSession = thomaz::load_session();`). No other changes to main.cpp are needed.

Note: `MAIN_SRC` is populated via `file(GLOB_RECURSE ...)` in CMakeLists.txt, so `boot_activity.cpp` is automatically included in the build. XML resources are served from `PROJECT_RESOURCES` directory, which already covers `resources/xml/activity/boot.xml`.
  </action>
  <verify>
    <automated>cd /home/solid/www/personal/playground/thomas && cmake --build build-desktop --target thomaz 2>&1 | grep -E "error:|Built target thomaz"</automated>
  </verify>
  <done>main.cpp conditionally pushes BootActivity when restoredSession is empty; desktop build target `thomaz` compiles successfully with no errors.</done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| User input -> AuthActivity | Username/password entered in the auth form (handled by existing AuthActivity — no new input surface introduced) |
| Session file -> load_session() | Persisted session token read from SD card / local file |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-qgb-01 | Spoofing | BootActivity session-skip branch | accept | restoredSession comes from load_session() which reads the same auth_store file already trusted by the existing flow; no new attack surface |
| T-qgb-02 | Tampering | boot.xml layout | accept | XML is ROM-packed (libromfs) on Switch; desktop is local dev only |
| T-qgb-03 | Denial of Service | BootActivity holding service pointers | accept | Service lifetimes are owned by main() stack; BootActivity is popped before goHome() pushes HomeActivity, so no double-free risk |
</threat_model>

<verification>
After all three tasks complete, verify the full boot flow manually on desktop:

1. Delete any saved session file (`thomaz_session.json` in the working directory) and launch — BootActivity should appear with black background, icon left, two buttons right.
2. Press "Entrar" — AuthActivity should push on top; logging in should pop AuthActivity and push HomeActivity.
3. Press "Continuar sem login" — HomeActivity should open directly.
4. With a session file present, launch — BootActivity should be skipped and HomeActivity opens directly (same as pre-change behavior).
</verification>

<success_criteria>
- `resources/xml/activity/boot.xml` exists with the described two-column layout
- `source/app/boot_activity.hpp` and `.cpp` compile cleanly
- All five i18n files contain `thomaz/boot/login` and `thomaz/boot/guest` keys
- `source/main.cpp` conditionally pushes BootActivity vs HomeActivity based on `restoredSession.has_value()`
- Desktop build: `cmake -DUSE_SDL2=ON` + `cmake --build` produces `thomaz` target with no errors
</success_criteria>

<output>
Create `.planning/quick/260605-qgb-vamos-adicionar-uma-bootscreen-no-app-us/260605-qgb-SUMMARY.md` when done.
</output>
