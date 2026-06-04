# Testing Patterns

**Analysis Date:** 2026-06-04

## Test Framework

### TypeScript / API

**Runner:** Vitest 3.x
- Config: `api/vitest.config.ts`
- Environment: `node`
- File parallelism: disabled (`fileParallelism: false`) — tests share a real Postgres DB
- Timeouts: 30 seconds for both hooks and tests

**Assertion Library:** Vitest built-in (`expect`)

**Run Commands:**
```bash
cd api && npm test           # Run all tests (vitest run)
cd api && npm run test:watch # Watch mode (vitest)
# No coverage command configured
```

### C++ / NRO App

**Runner:** doctest (header-only)
- Header: `lib/doctest/doctest.h`
- Entry: `tests/test_main.cpp` — defines `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`
- Build: `g++ -std=c++17 -Wall -Wextra -I../lib/doctest -I../lib/json -I../source`

**Assertion Library:** doctest built-in (`CHECK`, `REQUIRE`, `CHECK_FALSE`)

**Run Commands:**
```bash
cd tests && make test   # Build and run all C++ tests
cd tests && make clean  # Remove test binary
./tests/run            # Run already-built binary directly
```

## Test File Organization

### TypeScript

**Location:** `api/test/` directory (separate from `api/src/`)

**Naming:** `<module>.test.ts` (e.g., `api.test.ts`)

**Structure:**
```
api/
├── src/
│   └── app.ts          # buildApp() factory consumed by tests
└── test/
    └── api.test.ts     # Single integration test file
```

### C++

**Location:** `tests/` directory at repo root, separate from `source/`

**Naming:** `test_<module>.cpp` (e.g., `test_cheat_txt.cpp`, `test_mod_install.cpp`)

**Structure:**
```
tests/
├── test_main.cpp            # doctest entry point (DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN)
├── test_cheat_txt.cpp       # Tests for source/core/cheat_txt.*
├── test_mod_install.cpp     # Tests for source/core/mods/mod_install.*
├── test_save_sync.cpp       # Tests for source/core/saves/save_sync.*
├── test_themezer_json.cpp   # Tests for source/core/themes/themezer_json.*
├── test_sysmod_scan.cpp     # Tests for source/core/sysmod/sysmod_scan.*
└── ...                      # One file per module (~35 test files total)
```

## Test Structure

### TypeScript

Single `describe` block wrapping named `it` cases:

```typescript
// api/test/api.test.ts

beforeAll(async () => {
  // Set env vars, create temp upload dir, call buildApp()
  uploadDir = await mkdtemp(join(tmpdir(), "thomaz-api-"));
  const built = await buildApp();
  app = built.app;
});

afterAll(async () => {
  await app.close();
  await rm(uploadDir, { recursive: true, force: true });
});

describe("thomaz-api", () => {
  it("GET /health", async () => {
    const res = await app.inject({ method: "GET", url: "/health" });
    expect(res.statusCode).toBe(200);
    expect(res.json()).toEqual({ status: "ok" });
  });
});
```

### C++

Each `test_<module>.cpp` file contains multiple `TEST_CASE` blocks with no test class nesting:

```cpp
// tests/test_cheat_txt.cpp

#include "doctest.h"
#include "core/cheat_txt.hpp"

using thomaz::core::parse_txt;

TEST_CASE("parse_txt splits regular and master cheats") {
    auto cheats = parse_txt(body);
    REQUIRE(cheats.size() == 3);
    CHECK(cheats[0].is_master == true);
    CHECK(cheats[0].name == "Master Code");
}

TEST_CASE("parse_txt on empty input returns no cheats") {
    CHECK(parse_txt("").empty());
}
```

**Patterns:**
- `REQUIRE` used for preconditions that would crash subsequent checks if false
- `CHECK` used for all other assertions (test continues on failure)
- `CHECK_FALSE` for negative assertions
- Test case names are descriptive full sentences

## Mocking

### TypeScript

No mocking framework used. The API test suite uses **real infrastructure**:
- Real PostgreSQL database (via `DATABASE_URL` env, defaults to `localhost:5433/thomaz`)
- Real filesystem temp directory for file uploads
- Fastify's `app.inject()` for HTTP requests (no network, but real app logic)

**What is NOT mocked:**
- Database (Prisma client hits real Postgres)
- Filesystem (real `mkdtemp`, file reads/writes)
- JWT signing/verification (real `@fastify/jwt`)
- Password hashing (real `argon2`)

**Rate-limit isolation pattern:**
```typescript
// Build a separate app instance with custom config for isolated tests
const built = await buildApp({ AUTH_RATE_MAX: 3 });
const rlApp = built.app;
try {
  // ... test against rlApp
} finally {
  await rlApp.close();
}
```

### C++

Fake implementations used for platform-dependent code:

- `source/platform/saves/fake_cloud_save_client.cpp` — implements `cloud_save_client.hpp` interface without network
- `source/platform/title_service_fake.cpp` — fake title service for desktop/test builds
- `source/platform/save_service_fake.cpp` — fake save service

Test files link against fake implementations (see `tests/Makefile` SRCS list). Core logic (`source/core/`) is pure and testable without any fakes.

**What is mocked/faked:**
- Network clients (HTTP cloud save, feed client) — replaced with fake implementations
- Switch-specific system services (title service, save service) — replaced with fake implementations

**What is NOT mocked:**
- JSON parsing (uses real `nlohmann/json` via `lib/json/`)
- String/path manipulation
- Business logic in `source/core/`

## Fixtures and Factories

### TypeScript

Test data created inline with unique identifiers using `Date.now()`:

```typescript
const user = `u_${Date.now()}`;   // Unique username per test run
const pass = "password1";
// Register and use; no shared fixtures
```

Binary fixture data created inline:
```typescript
const jpeg = Buffer.from([0xff, 0xd8, 0xff, 0xd9]);  // Minimal valid JPEG
const blob = Buffer.from("encrypted-save-blob-v1");   // Arbitrary binary
```

**Location:** Inline within test cases — no shared fixture files.

### C++

Test input strings defined as inline `const std::string` literals within `TEST_CASE` blocks:

```cpp
TEST_CASE("parse_txt splits regular and master cheats") {
    const std::string body =
        "{Master Code}\n"
        "580F0000 0149D940\n"
        "\n"
        "[Infinite Health Save 1]\n"
        "11160000 5C3BE7DC 00000006\n";
    auto cheats = parse_txt(body);
    // assertions...
}
```

Static helper lambdas used for repeated construction:
```cpp
// tests/test_mod_install.cpp
static ArchiveEntry f(const std::string& p) { return ArchiveEntry{p, false}; }
static ArchiveEntry d(const std::string& p) { return ArchiveEntry{p, true}; }
```

## Coverage

**Requirements:** None enforced (no coverage config in `vitest.config.ts`, no CI coverage gate observed)

**View Coverage:**
```bash
cd api && npx vitest run --coverage  # Not pre-configured; must add @vitest/coverage-v8
```

## Test Types

**Unit Tests (C++):**
- All `tests/test_*.cpp` files are unit tests of individual `source/core/` and `source/platform/` modules
- Pure function inputs/outputs, no I/O
- Coverage: ~35 test files covering cheats, mods, saves, themes, sysmodules, feeds, session, update

**Integration Tests (TypeScript):**
- `api/test/api.test.ts` is a full integration test hitting a live database
- Tests complete HTTP flows: register → login → create resource → read → delete
- Tests auth edge cases: duplicate registration, token rotation, rate limiting, authorization (403 forbidden)

**E2E Tests:** Not used — no browser/device automation framework detected.

## Common Patterns

**Async Testing (TypeScript):**
```typescript
it("saves list, upload, download with revision", async () => {
  const res = await app.inject({ method: "PUT", url: `/saves/${titleId}`, ... });
  expect(res.statusCode).toBe(200);
});
```

**Error/Rejection Testing (TypeScript):**
```typescript
// Test HTTP error codes directly from inject()
const conflict = await app.inject({ method: "PUT", url: `/saves/${titleId}`, payload: formConflict });
expect(conflict.statusCode).toBe(409);
expect(conflict.json()).toEqual({ ok: false, error: "revision_conflict" });
```

**Struct/Enum Error Testing (C++):**
```cpp
// Check error variant on result struct
CHECK(plan_install({}).error == InstallError::Empty);
CHECK(plan_install({f("exefs/main.npdm")}).error == InstallError::NotRomfs);

// Check success path via .ok()
InstallPlan p = plan_install({d("romfs/"), f("romfs/Actor/foo.bin")});
REQUIRE(p.ok());
CHECK(p.strip_prefix == "");
```

**Negative Assertion (C++):**
```cpp
CHECK_FALSE(is_safe_archive_path(""));
CHECK_FALSE(is_safe_archive_path("/etc/passwd"));
CHECK_FALSE(is_safe_archive_path("romfs/../../evil"));
```

---

*Testing analysis: 2026-06-04*
