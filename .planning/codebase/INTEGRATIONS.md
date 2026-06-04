# External Integrations

**Analysis Date:** 2026-06-04

## APIs & External Services

**Thomaz API (self-hosted):**
- Purpose: Cloud saves sync, social feed (posts/likes/comments), user auth
- Base URL: `https://api.thomaz.baseup.cc` (hardcoded default in `source/platform/app_settings.cpp`; overridable via per-platform config file)
- Client code: `source/core/saves/save_sync.cpp`, `source/core/feed/feed_json.cpp`, `source/app/auth_activity.cpp`

**switch-cheats-db (GitHub raw):**
- Purpose: Download cheat codes per game title ID
- Base URL: `https://raw.githubusercontent.com/HamletDuFromage/switch-cheats-db/master` (hardcoded in `source/core/db_paths.cpp`)
- Endpoints used:
  - `/versions.json` — root index (~3.4 MB)
  - `/cheats/<TITLE_ID_UPPER>.json` — per-title cheat list
  - `/versions/<TITLE_ID_UPPER>.json` — per-title build-version map
- Client code: `source/core/db_paths.cpp`, `source/platform/cheat_store.cpp`
- Auth: None (public GitHub raw CDN)

**GameBanana API v11:**
- Purpose: Mod browser — search mods/games, list mod files for download
- Base URL: `https://gamebanana.com/apiv11` (hardcoded in `source/core/mods/gamebanana_urls.cpp`)
- Endpoints used:
  - `/Util/Search/Results` — keyword + game-id mod search
  - `/Mod/<mod_id>?_csvProperties=_aFiles` — fetch download URLs for a mod
  - `/Game/<game_id>/Subfeed` — game-specific mod feed
  - `/Util/Search/Results?_sModelName=Game` — game search
- Client code: `source/core/mods/gamebanana_urls.cpp`, `source/core/mods/mod_browse.cpp`
- Auth: None (public API)

**Themezer GraphQL API:**
- Purpose: Browse and download NX Switch themes
- Endpoint: `https://api.themezer.net/graphql` (hardcoded in `source/app/theme_browser_activity.cpp` line 29)
- Protocol: GraphQL over HTTPS (POST with JSON body)
- Client code: `source/app/theme_browser_activity.cpp`, `source/core/themes/themezer_browse.cpp`, `source/core/themes/themezer_query.cpp`
- Auth: None (public API)

**GitHub Releases API:**
- Purpose: Self-update check for the NRO binary
- Endpoint: `https://api.github.com/repos/luizfbalves/thomaz/releases/latest` (hardcoded in `source/core/update.cpp`)
- Client code: `source/core/update.cpp`
- Auth: None (public endpoint, unauthenticated rate limit applies)

## Data Storage

**Databases:**
- PostgreSQL 16
  - ORM: Prisma 6.8 (`api/prisma/schema.prisma`)
  - Connection: `DATABASE_URL` env var
  - Schema models: `User`, `RefreshToken`, `Post`, `Like`, `Comment`, `SaveSlot`
  - Migrations: `api/prisma/migrations/`

**File Storage:**
- Local filesystem on Lightsail instance
  - Upload directory configured via `UPLOAD_DIR` env var (default `./uploads`)
  - Served statically via `@fastify/static` at `PUBLIC_BASE_URL`
  - Used for: post images, cloud save blobs (`blobKey` column in `SaveSlot`)
  - No object storage (S3/R2) — direct disk writes via `api/src/lib/storage.ts`

**Caching:**
- None (server-side)
- Client-side: switch-cheats-db index cached on SD card / local disk by `source/platform/cheat_store.cpp`

## Authentication & Identity

**Auth Provider: Custom (self-built)**
- Mechanism: username + Argon2id password hash (`api/src/lib/passwords.ts`)
- Session: dual-token scheme
  - Access token: JWT (HS256) via `@fastify/jwt`, default TTL 365 days (console UX constraint, configurable via `JWT_ACCESS_EXPIRES`)
  - Refresh token: opaque token, hashed with Argon2id, stored in `RefreshToken` table, default TTL 365 days (`REFRESH_TOKEN_TTL_DAYS`)
- Token handling: `api/src/lib/auth-tokens.ts`, `api/src/lib/refresh-tokens.ts`
- Auth plugin: `api/src/plugins/auth.ts` — decorates Fastify with `authenticate` and `optionalAuth` hooks
- No third-party OAuth / social login

## Monitoring & Observability

**Error Tracking:** None detected

**Logs:**
- API: `console.error` for startup config errors (`api/src/config.ts`); Fastify built-in request logging (pino, default Fastify behavior)
- Client: Borealis framework logging (platform-specific)

## CI/CD & Deployment

**Hosting:**
- AWS Lightsail — API production server
- Domain: `api.thomaz.baseup.cc`
- Process manager: PM2

**CI Pipeline:**
- GitHub Actions — workflow at `.github/workflows/api.yml`
- Triggers: push or PR touching `api/**` or `.github/workflows/api.yml`
- Jobs:
  1. `test` — spins up `postgres:16-alpine` service container, runs `prisma migrate deploy`, `typecheck`, `vitest run`
  2. `deploy` — SSH deploy to Lightsail via `appleboy/ssh-action@v1.2.0` (runs only on push to `main` after tests pass)
- Required GitHub Secrets: `LIGHTSAIL_HOST`, `LIGHTSAIL_USER`, `LIGHTSAIL_SSH_KEY`

## Environment Configuration

**Required env vars (API):**
- `DATABASE_URL` — PostgreSQL connection string
- `JWT_SECRET` — minimum 16 characters
- `PUBLIC_BASE_URL` — public HTTPS base URL for image links

**Optional env vars with defaults:**
- `PORT` — default `3000`
- `HOST` — default `0.0.0.0`
- `UPLOAD_DIR` — default `./uploads`
- `CORS_ORIGINS` — comma-separated, default `http://localhost:3000`
- `JWT_ACCESS_EXPIRES` — default `365d`
- `REFRESH_TOKEN_TTL_DAYS` — default `365`
- `AUTH_RATE_MAX` — default `10` (requests/min per IP on auth endpoints)
- `NODE_ENV` — `development` | `production` | `test`

**Secrets location:**
- `api/.env` (not committed; `.env.example` committed as template)
- Production secrets in GitHub repo secrets for CI deploy

## Webhooks & Callbacks

**Incoming:** None

**Outgoing:** None (client initiates all external calls)

## Nintendo Switch Platform IPC

**Atmosphère / dmnt:cht (cheat engine):**
- Client uses dmnt IPC to apply cheat codes to running games
- No direct HTTP; operates via Switch kernel IPC channels
- Integration code: `source/platform/` (Switch-only, guarded by `#ifdef __SWITCH__`)

**Sysmodule management:**
- Toggle sysmodules by writing/removing `boot2.flag` files under `/atmosphere/contents/<program_id>/flags/`
- Reads `toolbox.json` from each sysmodule directory for metadata (`source/core/sysmod/toolbox_json.cpp`)
- No external API; pure SD card filesystem manipulation

---

*Integration audit: 2026-06-04*
