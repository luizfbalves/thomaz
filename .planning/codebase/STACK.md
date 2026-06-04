# Technology Stack

**Analysis Date:** 2026-06-04

## Languages

**Primary (NRO / Desktop client):**
- C++20 — all application logic under `source/`; C++20 required for vendored `lib/switchthemes` (uses `std::span`, `operator<=>`, `std::format`)

**Primary (API backend):**
- TypeScript 5.8 (strict) — all source under `api/src/`

**Secondary:**
- C (C11) — Switch platform wrapper `lib/borealis/library/lib/platforms/switch/switch_wrapper.c`

## Runtime

**Client:**
- Nintendo Switch: devkitPro / devkitA64 toolchain (bare-metal homebrew NRO)
- Desktop: Linux/macOS native binary (SDL2 or GLFW backend)

**API:**
- Node.js ≥20 (ESM — `"type": "module"` in `api/package.json`)

**Package Manager (API):**
- npm
- Lockfile: `api/package-lock.json` (present, committed)

## Frameworks

**Client UI:**
- Borealis (xfangfang fork) — Nintendo Switch / desktop UI framework; vendored at `lib/borealis/`
- Build system: CMake 3.10+ via `CMakeLists.txt`; targets `thomaz.nro` (Switch) and `thomaz` ELF (desktop)

**API:**
- Fastify 5.3 — HTTP server (`api/src/app.ts`, `api/src/index.ts`)
- Prisma 6.8 — ORM and migration runner (`api/prisma/schema.prisma`)
- Zod 3.25 — runtime env and request validation (`api/src/config.ts`, route handlers)

**Testing (API):**
- Vitest 3.1 — test runner (`api/vitest.config.*`, `api/test/`)

**Build/Dev (API):**
- tsx 4.19 — TypeScript dev runner (`npm run dev`)
- @swc/cli + @swc/core 1.15 — production transpile (`npm run build` → `api/dist/`)
- tsc — type-check only (`npm run typecheck`)

## Key Dependencies

**API — Critical:**
- `@fastify/jwt` 9.1 — JWT access-token issuance and verification (`api/src/plugins/auth.ts`)
- `@fastify/multipart` 9.0 — multipart file upload for post images (`api/src/routes/posts.ts`)
- `@fastify/rate-limit` 10.2 — brute-force guard on `/auth/login` and `/auth/register`
- `@fastify/cors` 11.0 — CORS with configurable origins from `CORS_ORIGINS` env
- `@fastify/static` 8.1 — serves uploaded images from `UPLOAD_DIR`
- `argon2` 0.43 — password hashing (`api/src/lib/passwords.ts`)
- `@prisma/client` 6.8 — database client (PostgreSQL)

**Client — Critical (Switch portlibs):**
- libcurl + mbedtls/mbedx509/mbedcrypto — HTTPS for all outgoing API calls (`source/platform/http_client_curl.cpp`)
- libarchive + zstd + lz4 + bz2 + lzma + zlib — archive extraction for mod/theme downloads
- nlohmann/json (header-only) — JSON parsing, vendored at `lib/json/nlohmann/`
- switchthemes engine (GPLv2 exelix11 fork) — NX theme patching, vendored at `lib/switchthemes/`

**Client — Desktop only:**
- CURL::libcurl (system) + LibArchive::LibArchive (system) — found via CMake `find_package`

## Configuration

**API Environment:**
- Validated at startup by Zod in `api/src/config.ts`
- Example file: `api/.env.example`
- Runtime: `node --env-file=.env dist/index.js` (native Node env-file loading, no dotenv package)
- Key variables: `DATABASE_URL`, `JWT_SECRET`, `PUBLIC_BASE_URL`, `UPLOAD_DIR`, `CORS_ORIGINS`, `JWT_ACCESS_EXPIRES`, `REFRESH_TOKEN_TTL_DAYS`, `AUTH_RATE_MAX`

**Client Runtime Config (persisted on SD / local disk):**
- Locale: `/switch/thomaz/config/locale.txt` (Switch) or `thomaz-cache/locale.txt` (desktop)
- API base URL override: `/switch/thomaz/config/api_url.txt` (Switch) or `thomaz-cache/api_url.txt` (desktop)
- Default API base: `https://api.thomaz.baseup.cc`

**Build:**
- `CMakeLists.txt` — root CMake config; key options: `USE_SDL2`, `USE_LIBROMFS`, `USE_DEKO3D`, `USE_SHARED_LIB`
- Toolchain auto-detected via `lib/borealis/library/cmake/toolchain.cmake`; `PLATFORM_SWITCH` set when devkitPro present

## Platform Requirements

**Development (API):**
- Node.js ≥20, npm
- PostgreSQL 16 (CI uses `postgres:16-alpine`)

**Development (Client — Switch):**
- devkitPro devkitA64 + switch-portlibs (`switch-curl`, `switch-mbedtls`, `switch-zlib`, `switch-libarchive`)

**Development (Client — Desktop):**
- CMake + C++20 compiler
- `-DUSE_SDL2=ON` required (GLFW/wayland-scanner path fails on Linux without it)
- System libcurl, libarchive

**Production (API):**
- AWS Lightsail — live at `api.thomaz.baseup.cc`
- Process manager: PM2 (`npm run start`)
- Auto-deploy on push to `main` targeting `api/**` via GitHub Actions SSH (`appleboy/ssh-action`)

---

*Stack analysis: 2026-06-04*
