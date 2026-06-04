# Coding Conventions

**Analysis Date:** 2026-06-04

## Naming Patterns

**Files:**
- C++ source: `snake_case.cpp` / `snake_case.hpp` (e.g., `cheat_txt.cpp`, `mod_install.hpp`)
- C++ test files: `test_<module>.cpp` (e.g., `test_cheat_txt.cpp`, `test_mod_install.cpp`)
- TypeScript source: `kebab-case.ts` (e.g., `auth-tokens.ts`, `save-storage.ts`, `refresh-tokens.ts`)
- TypeScript routes: `<resource>.ts` (e.g., `auth.ts`, `saves.ts`, `posts.ts`)

**Functions (C++):**
- Free functions: `snake_case` (e.g., `parse_txt`, `serialize_txt`, `parse_header`, `enabled_cheat_names`)
- Helper types/structs: `PascalCase` (e.g., `ModRecord`, `SearchPage`, `InstallPlan`, `ArchiveEntry`)
- Enum values: `PascalCase` (e.g., `InstallError::Empty`, `InstallError::NotRomfs`)

**Functions (TypeScript):**
- Exported functions: `camelCase` (e.g., `buildApp`, `loadConfig`, `corsOrigins`, `signAuthResponse`)
- Route registrators: `<resource>Routes` naming (e.g., `authRoutes`, `savesRoutes`, `feedRoutes`)
- Serializer functions: `to<Type>Dto` naming (e.g., `toPostDto`, `toSaveSlotDto`, `toUserDto`, `toCommentDto`)
- Error helpers: `<domain>Error` naming (e.g., `authError`, `actionError`, `errorBody`)

**Variables (C++):**
- Local variables: `snake_case`
- Struct members: `snake_case` with descriptive names mapping to JSON fields (documented in comments)

**Variables (TypeScript):**
- Local variables: `camelCase`
- Constants: `SCREAMING_SNAKE_CASE` for schema names (e.g., `credentialsSchema`, `envSchema`)
- Type exports: `PascalCase` (e.g., `Env`, `PostWithCounts`, `SaveSlot`)

**C++ Namespaces:**
- All project code lives under `thomaz::` with subdomain nesting: `thomaz::core`, `thomaz::core::` (implied for types in same namespace)
- Anonymous namespaces used for file-local helpers (e.g., `trim`, `parse_header` in `cheat_txt.cpp`)

## Code Style

**Formatting (TypeScript):**
- No formatter config file detected (no `.prettierrc`, `.eslintrc`, `biome.json` at root or api/)
- Inferred from source: trailing commas, 2-space indentation, single quotes for strings

**Linting:**
- No ESLint config detected
- TypeScript strict mode enabled (`"strict": true` in `api/tsconfig.json`)
- `skipLibCheck: true`

**C++ Standard:**
- C++17 (`-std=c++17` in `tests/Makefile`)
- Compiler flags: `-Wall -Wextra`

## Import Organization (TypeScript)

**Order:**
1. Node built-ins with `node:` prefix (e.g., `import { mkdtemp } from "node:fs/promises"`)
2. Third-party packages (e.g., `import Fastify from "fastify"`, `import { z } from "zod"`)
3. Local project imports with `.js` extension (e.g., `import { buildApp } from "../src/app.js"`)

**Extension requirement:** All local TypeScript imports use `.js` extension (NodeNext module resolution)

**Path Aliases:** None detected — relative paths used throughout.

## Error Handling

**TypeScript (API):**
- Errors returned as JSON objects: `{ ok: false, error: "<error_code>" }` — never throw HTTP errors, always `reply.status(N).send(...)`
- Three typed helper functions all returning same shape: `errorBody`, `authError`, `actionError` (in `api/src/lib/errors.ts`)
- Error codes are `snake_case` strings (e.g., `"invalid_credentials"`, `"revision_conflict"`, `"invalid_title_id"`)
- Input validation via Zod: `.safeParse()` used (never `.parse()` — avoids thrown exceptions at route level)
- Auth guard: `preHandler: [app.authenticate]` + explicit `userIdFromRequest` null check pattern

**C++:**
- Return value semantics: structs with `.ok()` method or error enum member (e.g., `InstallPlan.ok()`, `InstallPlan.error`)
- Error enum used for structured failure reporting (e.g., `InstallError::Empty`, `InstallError::NotRomfs`, `InstallError::UnsafePath`)
- No exceptions observed in project code

## Logging

**TypeScript:**
- Fastify logger disabled in tests (`logger: false` in `buildApp`)
- `console.error` used only for environment config failures (`api/src/config.ts`)
- No structured logging library; production log strategy not observed in source

**C++:** No logging framework observed; console output not used in core logic.

## Comments

**C++:**
- File-level comments on `.hpp` files explaining each exported function's contract
- Inline comments explain JSON field mapping in struct definitions (e.g., `// _idRow`, `// _sName`)
- Anonymous namespace helpers have brief inline comments

**TypeScript:**
- Block comments on non-obvious business logic (e.g., rate limit rationale in `auth.ts`, JWT expiry reasoning in `config.ts`)
- No JSDoc/TSDoc annotations observed

## Function Design

**C++:**
- Free functions preferred over methods; pure functions where possible
- Structs are plain data containers, logic lives in free functions
- `#pragma once` used for all headers (no include guards)

**TypeScript:**
- `async function` syntax used throughout (no arrow-function module exports)
- Route handlers are async closures passed directly to Fastify route registration
- Zod schemas defined at module scope as `const` above the function that uses them

## Module Design

**TypeScript:**
- Each route file exports one `async function <resource>Routes(app, env): Promise<void>`
- Lib files export pure utility functions (no classes)
- Plugins (`api/src/plugins/`) register Fastify decorators; accessed via `app.prisma`, `app.authenticate`
- `api/src/app.ts` exports a single `buildApp(overrides?)` factory used by both the server entry and tests
- ESM modules throughout (`"type": "module"` in `package.json`)

**C++:**
- `source/core/` contains pure logic with no platform dependencies
- `source/platform/` contains I/O, HTTP, and Switch-specific code
- `source/app/` contains Borealis UI activities
- Headers are self-contained with `#pragma once`

---

*Convention analysis: 2026-06-04*
