# Phase 2: API Security + Regression Tests - Pattern Map

**Mapped:** 2026-06-04
**Files analyzed:** 7 modified + 1 new (migration)
**Analogs found:** 8 / 8 (all in-repo, exact or strong role-match)

This phase is additive wiring — every file being modified already exists and every
new artifact (the `RevokedToken` model + migration) has a near-exact analog in the
same repo (`RefreshToken`). The planner should copy from the analogs cited below
rather than from RESEARCH.md examples (which were themselves derived from these files).

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `api/prisma/schema.prisma` (+`RevokedToken`) | model | CRUD | `RefreshToken` model (same file, lines 19-28) | exact |
| `api/prisma/migrations/<ts>_revoked_tokens/migration.sql` (NEW) | migration | CRUD | `migrations/20250603150000_refresh_tokens/migration.sql` | exact |
| `api/src/plugins/auth.ts` (blocklist check + `JwtPayload` fields) | middleware/plugin | request-response | `authenticate` decorator (same file, lines 37-46) | exact (extend-in-place) |
| `api/src/routes/auth.ts` (`/auth/logout` revoke; `/auth/refresh` jti) | route | request-response | `/auth/logout` + `/auth/refresh` handlers (same file, lines 73-111) | exact (extend-in-place) |
| `api/src/lib/auth-tokens.ts` (`signAuthResponse` adds jti) | service/lib | transform | `signAuthResponse` (same file, lines 6-18) | exact (extend-in-place) |
| `api/src/lib/revoked-tokens.ts` (OPTIONAL new helper) | service/lib | CRUD | `api/src/lib/refresh-tokens.ts` (whole file) | exact (mirror) |
| `api/src/app.ts` (`logger: false` → `envToLogger`) | config | n/a | `Fastify({ logger: false })` (same file, line 24) | role-match |
| `api/src/config.ts` (NODE_ENV enum — already present) | config | n/a | `envSchema` (same file, lines 3-23) | exact (read-only ref) |
| `api/test/api.test.ts` (TEST-01, TEST-02, SEC-02) | test | request-response | existing `app.inject()` tests (same file) | exact (extend-in-place) |

---

## Pattern Assignments

### `api/prisma/schema.prisma` — add `RevokedToken` model (model, CRUD)

**Analog:** `RefreshToken` model in the same file (lines 19-28). Copy its shape but
drop the `User` relation/FK per D-07 (bare `userId` string, relationless).

**Existing analog** (`api/prisma/schema.prisma:19-28`):
```prisma
model RefreshToken {
  id        String   @id @default(cuid())
  userId    String
  user      User     @relation(fields: [userId], references: [id], onDelete: Cascade)
  tokenHash String   @unique
  expiresAt DateTime
  createdAt DateTime @default(now())

  @@index([userId])
}
```

**New model to add** (D-07: `jti` is the PK, no `User` relation, index on `expiresAt`):
```prisma
model RevokedToken {
  jti       String   @id
  userId    String
  expiresAt DateTime
  createdAt DateTime @default(now())

  @@index([expiresAt])
}
```
Note: D-07 mandates only `@@index([expiresAt])`. `@@index([userId])` is optional
(enables the deferred "revoke all sessions" feature) — planner's call, not required.

---

### `api/prisma/migrations/<ts>_revoked_tokens/migration.sql` (NEW) — migration, CRUD

**Analog:** `api/prisma/migrations/20250603150000_refresh_tokens/migration.sql`
(whole file). The repo uses **hand-numbered, versioned** migrations applied via
`prisma migrate deploy` in `deploy.sh` (NOT `db push`). Existing dirs:
`20250603120000_init`, `20250603140000_save_slot_blob`, `20250603150000_refresh_tokens`,
`20260604233032_remove_community_models`. Prefer `prisma migrate dev --name revoked_tokens`
to auto-generate a timestamp-prefixed dir + equivalent SQL.

**Existing analog** (`migrations/20250603150000_refresh_tokens/migration.sql:1-16`):
```sql
-- CreateTable
CREATE TABLE "RefreshToken" (
    "id" TEXT NOT NULL,
    "userId" TEXT NOT NULL,
    "tokenHash" TEXT NOT NULL,
    "expiresAt" TIMESTAMP(3) NOT NULL,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "RefreshToken_pkey" PRIMARY KEY ("id")
);

-- CreateIndex
CREATE INDEX "RefreshToken_userId_idx" ON "RefreshToken"("userId");
```
(The `RefreshToken` migration also has a UNIQUE index + AddForeignKey; `RevokedToken`
needs neither — no `tokenHash` unique, no FK.)

**New SQL to produce** (PK on `jti`, plain index on `expiresAt`):
```sql
-- CreateTable
CREATE TABLE "RevokedToken" (
    "jti" TEXT NOT NULL,
    "userId" TEXT NOT NULL,
    "expiresAt" TIMESTAMP(3) NOT NULL,
    "createdAt" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "RevokedToken_pkey" PRIMARY KEY ("jti")
);

-- CreateIndex
CREATE INDEX "RevokedToken_expiresAt_idx" ON "RevokedToken"("expiresAt");
```

---

### `api/src/plugins/auth.ts` — `JwtPayload` fields + blocklist check (middleware, request-response)

**Analog:** the `authenticate` decorator in the same file (lines 37-46). Extend the
existing try/catch — do not add a preHandler anywhere else (D-04: one decorator covers
all `preHandler:[app.authenticate]` routes).

**Current `JwtPayload` type** (`api/src/plugins/auth.ts:4-14`) — extend with optional `jti`/`exp` (Pitfall 5):
```typescript
export type JwtPayload = {
  sub: string;
  username: string;
};

declare module "@fastify/jwt" {
  interface FastifyJWT {
    payload: JwtPayload;
    user: JwtPayload;
  }
}
```
Add `jti?: string;` and `exp?: number;` to `JwtPayload` (optional — pre-deploy tokens
lack them; the module augmentation covers both `request.user` and `jwtVerify` generics).

**Current `authenticate` decorator** (`api/src/plugins/auth.ts:37-46`) — the exact insertion point:
```typescript
app.decorate(
  "authenticate",
  async function (request, reply) {
    try {
      await request.jwtVerify();
    } catch {
      return reply.status(401).send({ ok: false, error: "unauthorized" });
    }
  },
);
```

**Blocklist check to insert** after `jwtVerify()` succeeds (D-04/D-05/D-06/L-02; note
the 401 shape exactly matches the existing `catch` block — D-03 generic `unauthorized`):
```typescript
const { jti } = request.user as JwtPayload;
if (!jti) return;                       // D-05/L-02: pre-deploy token, no DB hit
try {
  const revoked = await app.prisma.revokedToken.findUnique({ where: { jti } });
  if (revoked) {
    return reply.status(401).send({ ok: false, error: "unauthorized" }); // D-03
  }
} catch (err) {
  request.log.warn({ err }, "revocation lookup failed; allowing");        // D-06 fail-open
}
```
`app.prisma` is the established accessor (used throughout `routes/saves.ts` and
`routes/auth.ts`). `request.log` is available once the logger is enabled (app.ts change).

---

### `api/src/routes/auth.ts` — `/auth/refresh` jti + `/auth/logout` best-effort revoke (route, request-response)

**Analog:** the two existing handlers in the same file. The `refreshBodySchema` +
`safeParse` + `authError("invalid_body")` envelope is already the established pattern
(lines 21-23, 104-107).

**`/auth/refresh` signing site** (`api/src/routes/auth.ts:95-100`) — add `jwtid`:
```typescript
const token = await reply.jwtSign(
  { sub: user.id, username: user.username },
  { expiresIn: env.JWT_ACCESS_EXPIRES },
);
```
Change the options object to `{ jwtid: randomUUID(), expiresIn: env.JWT_ACCESS_EXPIRES }`
and add `import { randomUUID } from "node:crypto";` (the same import style is used in
`lib/refresh-tokens.ts:1`).

**Current `/auth/logout` handler** (`api/src/routes/auth.ts:103-111`) — extend body only.
CRITICAL (Pitfall 4): this route has **no** `authRateLimit` config; keep it that way.
Do NOT add an `authenticate` preHandler (D-01, Anti-Pattern).
```typescript
app.post("/auth/logout", async (request, reply) => {
  const parsed = refreshBodySchema.safeParse(request.body);
  if (!parsed.success) {
    return reply.status(400).send(authError("invalid_body"));
  }

  await revokeRefreshToken(app.prisma, parsed.data.refreshToken);
  return { ok: true };
});
```

**Best-effort access-token revocation to insert** before `revokeRefreshToken` (D-01/D-08/D-09;
Pitfall 3: use `upsert` to survive double-logout P2002 — `revokeRefreshToken` + always-200 unchanged D-02):
```typescript
try {
  const decoded = await request.jwtVerify<JwtPayload>(); // reads Authorization header itself
  if (decoded.jti && decoded.exp) {
    await app.prisma.revokedToken.deleteMany({           // D-09 lazy sweep
      where: { expiresAt: { lt: new Date() } },
    });
    await app.prisma.revokedToken.upsert({               // Pitfall 3: upsert over create
      where: { jti: decoded.jti },
      create: {
        jti: decoded.jti,
        userId: decoded.sub,
        expiresAt: new Date(decoded.exp * 1000),         // D-08
      },
      update: {},
    });
  }
} catch {
  // missing/expired/invalid/no-jti bearer → do nothing, still return 200 (D-01)
}
```
Requires importing `JwtPayload` from `../plugins/auth.js`.

---

### `api/src/lib/auth-tokens.ts` — `signAuthResponse` adds jti (lib, transform)

**Analog:** `signAuthResponse` itself (lines 6-18) — second of the two access-token
signing sites (the first being `/auth/refresh`). Same `jwtid` edit.

**Current** (`api/src/lib/auth-tokens.ts:12-15`):
```typescript
const token = await reply.jwtSign(
  { sub: user.id, username: user.username },
  { expiresIn: env.JWT_ACCESS_EXPIRES },
);
```
Add `jwtid: randomUUID()` to the options object and `import { randomUUID } from "node:crypto";`.

---

### `api/src/lib/revoked-tokens.ts` (OPTIONAL new helper, lib, CRUD)

**Analog:** `api/src/lib/refresh-tokens.ts` (whole file) — same import header
(`node:crypto`, `PrismaClient`, `Env`), same `export async function (prisma, ...)`
shape, same Prisma-call-per-function style. If the planner extracts
`isJtiRevoked(prisma, jti)` / `revokeAccessJti(prisma, jti, userId, exp)` to keep
`auth.ts` thin, mirror this structure:
```typescript
import type { PrismaClient } from "@prisma/client";
// functions take (prisma, ...args) and return the Prisma promise result
export async function revokeRefreshToken(
  prisma: PrismaClient,
  rawToken: string,
): Promise<boolean> { /* ... */ }
```
Discretionary — the inline approach in `auth.ts`/`auth.ts` above is equally valid.

---

### `api/src/app.ts` — `envToLogger` map (config)

**Analog:** the single `Fastify({ logger: false })` line (line 24). No closer in-repo
analog exists (no logging is configured yet) — this is the one role-match drawing on
RESEARCH.md Pattern 4 + the Fastify Logging docs rather than existing code.

**Current** (`api/src/app.ts:24`):
```typescript
const app = Fastify({ logger: false });
```
`env` is already in scope (`const env = loadConfig();` at line 21) and `env.NODE_ENV`
is the validated enum from `config.ts`. Replace with a map keyed on `env.NODE_ENV`:
```typescript
const redact = { paths: ["req.headers.authorization", "req.headers.cookie"], remove: true };
const envToLogger = {
  development: { redact, transport: { target: "pino-pretty", options: { translateTime: "HH:MM:ss Z", ignore: "pid,hostname" } } },
  production: { redact },   // object (NOT `true`) so redact applies — Pitfall 2
  test: false,              // D-10: keeps Vitest suite silent (Success Criterion 4)
} as const;
const app = Fastify({ logger: envToLogger[env.NODE_ENV] });
```
Add `pino-pretty` as a **devDependency** (`cd api && npm install --save-dev pino-pretty`).
Do not add `pino` directly — Fastify 5 bundles it.

---

### `api/src/config.ts` — read-only reference (config)

**No change required.** The `NODE_ENV` enum already exists exactly as the logger map
needs (`api/src/config.ts:11-13`: `z.enum(["development","production","test"])`), and
`JWT_ACCESS_EXPIRES` defaults to `"365d"` (line 17). Cited so the planner knows the
logger-map key is already validated and no new env var is needed.

---

### `api/test/api.test.ts` — TEST-01, TEST-02, SEC-02 coverage (test, request-response)

**Analog:** the existing `app.inject()` tests in the same file. The `FormData` +
`form.getHeaders()` multipart pattern (lines 186-202, 241-281) is the template for
TEST-02; the register→token→`Authorization: Bearer` flow (lines 161-184) is the
template for SEC-02; the plain `app.inject({ method, url })` → status assertion
(lines 39-43) is the template for TEST-01.

**Multipart PUT pattern to copy** (`api/test/api.test.ts:186-203`) — TEST-02 reuses this,
omitting `form.append("revision", ...)` to hit the `revision_required` branch:
```typescript
const blob = Buffer.from("encrypted-save-blob-v1");
const form = new FormData();
form.append("data", blob, {
  filename: "save.bin",
  contentType: "application/octet-stream",
});
form.append("label", "slot-a");

const put = await app.inject({
  method: "PUT",
  url: `/saves/${titleId}`,
  headers: { authorization: `Bearer ${token}`, ...form.getHeaders() },
  payload: form,
});
expect(put.statusCode).toBe(200);
```

**Error-envelope assertion pattern to copy** (`api/test/api.test.ts:277-281`):
```typescript
expect(conflict.statusCode).toBe(409);
expect(conflict.json()).toEqual({ ok: false, error: "revision_conflict" });
```

**TEST-01 (SEC-01 guard)** — copy the plain-inject style (`api.test.ts:39-43`); assert 404:
```typescript
it("TEST-01: direct save-blob path is not publicly accessible", async () => {
  const res = await app.inject({
    method: "GET",
    url: "/uploads/saves/some-user-id/01008BB901469000.bin",
  });
  expect(res.statusCode).toBe(404); // no static route serves uploads/
});
```

**TEST-02 (revision_required, the only missing PUT branch)** — target `saves.ts:146-148`:
```typescript
// register → PUT once (creates slot, revision 1) → PUT again WITHOUT a revision field
const form = new FormData();
form.append("data", Buffer.from("v2"), { filename: "save.bin", contentType: "application/octet-stream" });
// intentionally omit form.append("revision", ...)
const res = await app.inject({
  method: "PUT", url: `/saves/${titleId}`,
  headers: { authorization: `Bearer ${token}`, ...form.getHeaders() },
  payload: form,
});
expect(res.statusCode).toBe(400);
expect(res.json()).toEqual({ ok: false, error: "revision_required" });
```

**SEC-02 (revoked-token rejection)** — combine the auth + saves patterns. Login the same
user twice (tokenA1, tokenA2); logout with tokenA1 in the `Authorization` header + its
refreshToken in the body; assert tokenA1→`GET /saves` is 401 `{ok:false,error:"unauthorized"}`
and tokenA2→`GET /saves` is still 200. (Note: a SEC-02 token only becomes revocable if it
carries a `jti` — i.e. issued by the new code — which all test-minted tokens will.)
**Test-env caveat:** `NODE_ENV=test` → `logger:false`, so the fail-open warn path and the
`findUnique` both run silently; no test-setup change needed for logging.

**Precondition (surface in verification steps):** the test Postgres at
`DATABASE_URL` (default `localhost:5433`) must have the new `RevokedToken` migration
applied (`prisma migrate deploy` against the test DB) before SEC-02 tests can pass —
the single most likely cause of a red suite if forgotten.

---

## Shared Patterns

### Error envelope (generic `unauthorized` and named codes)
**Source:** `api/src/lib/errors.ts:5-11` (`authError` / `actionError` both return
`{ ok: false, error: <code> }`).
**Apply to:** the blocklist 401 in `auth.ts` (D-03 reuses the literal
`{ ok: false, error: "unauthorized" }` shape already in the `authenticate` catch block);
all new test assertions use `expect(...json()).toEqual({ ok: false, error: <code> })`.
```typescript
export function authError(message: string): { ok: false; error: string } {
  return { ok: false, error: message };
}
```

### Prisma access via `app.prisma`
**Source:** used throughout `routes/saves.ts` (e.g. `app.prisma.saveSlot.findUnique`)
and `routes/auth.ts` (`app.prisma.user.findUnique`).
**Apply to:** `revokedToken.findUnique` (auth.ts decorator), `revokedToken.deleteMany`
+ `revokedToken.upsert` (logout). The model name lowercases to `app.prisma.revokedToken`.

### `node:crypto` import for token IDs
**Source:** `api/src/lib/refresh-tokens.ts:1` (`import { createHash, randomBytes } from "node:crypto";`).
**Apply to:** `import { randomUUID } from "node:crypto";` in both `auth-tokens.ts` and
`routes/auth.ts`. Don't hand-roll — `randomUUID()` is the collision-free, stateless choice.

### Hand-numbered versioned migrations (NOT `db push`)
**Source:** `api/prisma/migrations/` (four timestamp-prefixed dirs + `migration_lock.toml`);
`deploy.sh` runs `prisma migrate deploy` non-interactively.
**Apply to:** the new `RevokedToken` migration. Author with `prisma migrate dev --name
revoked_tokens` locally; the deploy path applies it via `migrate deploy`. Never `db push`.

### zod `safeParse` + early-return body validation
**Source:** `routes/auth.ts:104-107`, `routes/saves.ts:135-138`.
**Apply to:** the logout handler keeps its existing `refreshBodySchema.safeParse` guard
unchanged; the best-effort bearer logic is added *after* it.

---

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| `api/src/app.ts` (logger map) | config | n/a | No logging is configured yet in the repo; the `envToLogger`/`redact` shape comes from RESEARCH.md Pattern 4 + official Fastify Logging docs, not from an existing in-repo logger. Everything else (env access, `NODE_ENV` enum) is in-repo. |

All other files extend an exact in-repo analog.

## Metadata

**Analog search scope:** `api/src/` (plugins, routes, lib), `api/prisma/` (schema +
migrations), `api/test/`. Single-package backend; no broader search needed.
**Files scanned:** auth.ts (plugin), auth.ts (routes), auth-tokens.ts, refresh-tokens.ts,
app.ts, config.ts, schema.prisma, refresh_tokens migration.sql, saves.ts, api.test.ts,
errors.ts, migrations/ listing.
**Pattern extraction date:** 2026-06-04
