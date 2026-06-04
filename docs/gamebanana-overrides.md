# GameBanana game overrides

When you open **Get mods (GameBanana)** for a game, thomaz tries to match the
installed game to its GameBanana page by name. The match is intentionally
strict (exact name only) so one game never silently resolves to another — e.g.
*Resident Evil 4* must not resolve to *Resident Evil 4 Remake*. When there is no
exact match, thomaz falls back to a manual search instead of guessing.

If a game you own isn't matched automatically (or matches the wrong page), you
can pin it yourself with an **overrides file** — no rebuild needed.

## Where

Put the file at:

- **Switch:** `sd:/switch/thomaz/mods/overrides.json`
- **Desktop build:** `thomaz-mods/overrides.json` (relative to the working dir)

It's read fresh every time you open the mod browser, so edits apply without
restarting the app. Compiled-in verified pairs always win over this file.

## Format

```json
{
  "overrides": [
    { "title_id": "0100C2500FC20000", "game_id": 15056 },
    { "title_id": "01003BC0000A0000", "game_id": 6383 }
  ]
}
```

- `title_id` — the game's 16-hex Switch title ID, as a string.
- `game_id` — the GameBanana **game** ID (a number; a numeric string also works).

A bare array (`[ { ... }, { ... } ]`) is also accepted.

## How to find the IDs

- **Switch title ID:** look the game up on a title database (e.g. tinfoil.io),
  or read it from the game's detail screen in thomaz (the *Title ID* field).
- **GameBanana game ID:** open the game's page on gamebanana.com — the number in
  the URL `gamebanana.com/games/<id>` is the `game_id`. Make sure it's the
  *game* page, not the platform hub (`Nintendo Switch` = 6384 is the hub, not a
  game).

## Verified examples

| Game        | title_id           | game_id |
|-------------|--------------------|---------|
| Splatoon 3  | `0100C2500FC20000` | 15056   |
| Splatoon 2  | `01003BC0000A0000` | 6383    |

> The title IDs above are confirmed; double-check the `game_id` against the live
> GameBanana page before relying on it. For the Resident Evil titles, grab each
> game's title ID from its thomaz detail screen and its GameBanana game ID from
> the page URL, then add a row here.
