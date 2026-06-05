# Phase 2: API Security + Regression Tests - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-06-04
**Phase:** 2-api-security-regression-tests
**Areas discussed:** Logout contract, Blocklist enforcement, RevokedToken lifecycle, Logging policy (DEBT-04)

---

## Logout contract

### How /auth/logout obtains the access token

| Option | Description | Selected |
|--------|-------------|----------|
| Bearer + best-effort | Read access token from Authorization header if present; blocklist jti when valid; still revoke refreshToken; always 200, never 401 | ✓ |
| Require authenticate | Add app.authenticate preHandler; 401 if token invalid/expired | |
| Body carries both | Client sends accessToken + refreshToken in JSON body; decode jti, no header dependency | |

**User's choice:** Bearer + best-effort
**Notes:** Most forgiving for the C++ client; logout never fails. Still revokes the refresh token from the body.

### Reused-revoked-token UX

| Option | Description | Selected |
|--------|-------------|----------|
| 401 unauthorized | Generic `401 { ok:false, error:"unauthorized" }` — same shape as any invalid token; client's existing re-login path handles it | ✓ |
| Distinct error code | `401 token_revoked` so client can distinguish; needs C++ client branching | |

**User's choice:** 401 unauthorized
**Notes:** Avoids new client-side handling; the homebrew client already treats this shape as session-expired.

---

## Blocklist enforcement

### Failure mode when the RevokedToken lookup throws

| Option | Description | Selected |
|--------|-------------|----------|
| Fail-open (log + allow) | On DB error, log warning and allow the request; tiny revocation gap during a DB blip | ✓ |
| Fail-closed (401) | On DB error, reject with 401; a Postgres blip 401s all authed traffic | |

**User's choice:** Fail-open (log + allow)
**Notes:** Availability prioritized — a DB hiccup won't lock out every cloud-save user.

### Handling tokens with no jti

| Option | Description | Selected |
|--------|-------------|----------|
| Skip if no jti | No jti claim → allow with no DB hit | ✓ |
| Always query | Run findUnique even for jti-less tokens | |

**User's choice:** Skip if no jti
**Notes:** Honors the locked pre-deploy-token rule and avoids a wasted query per legacy-token request (up to 365d).

---

## RevokedToken lifecycle

### Cleanup of expired rows

| Option | Description | Selected |
|--------|-------------|----------|
| Lazy sweep on logout | deleteMany(expiresAt < now) before inserting; no scheduler | ✓ |
| Store expiresAt, never purge | Insert only, ignore expired on check; grows unbounded | |
| Scheduled purge | Periodic cron/job; extra operational moving part | |

**User's choice:** Lazy sweep on logout
**Notes:** Self-maintaining, cost amortized onto the logout path; no cron on the Lightsail/PM2 host.

### Table schema

| Option | Description | Selected |
|--------|-------------|----------|
| jti PK + expiresAt + userId | jti @id, userId, expiresAt, createdAt, @@index([expiresAt]) | ✓ |
| jti PK + expiresAt only | Bare minimum, no userId | |

**User's choice:** jti PK + expiresAt + userId
**Notes:** userId kept for audit / future "revoke all my sessions".

---

## Logging policy (DEBT-04)

### envToLogger per environment

| Option | Description | Selected |
|--------|-------------|----------|
| prod JSON / dev pretty / test off | production pino JSON, development pino-pretty, test false | ✓ |
| prod JSON / dev JSON / test off | both prod and dev JSON; avoids pino-pretty dep | |

**User's choice:** prod JSON / dev pretty / test off
**Notes:** Standard Fastify pattern; adds pino-pretty as a dev dependency.

### Redaction

| Option | Description | Selected |
|--------|-------------|----------|
| Redact headers.authorization | pino redact on req.headers.authorization + cookie; bodies not serialized by default | ✓ |
| Redact headers + custom serializer | Also whitelist req serializer to method/url/ip | |

**User's choice:** Redact headers.authorization
**Notes:** Minimal; Fastify's default serializer never logs request bodies, so passwords/usernames are not exposed.

---

## Claude's Discretion

- `jti` generation mechanism (`randomUUID()` vs `@fastify/jwt` jwtid) — must be added in both `signAuthResponse` and the `/auth/refresh` inline `jwtSign`.
- TEST-01 fixture shape (asserting a direct `/uploads/saves/...` path returns 404).
- TEST-02 test organization (the four revision branches already exist in code).
- Commit granularity, migration naming, test-file layout.

## Deferred Ideas

- "Revoke all my sessions" (bulk revocation by userId) — userId column added now to enable it later.
- Scheduled/cron purge of RevokedToken — rejected in favor of lazy sweep.
- Distinct `token_revoked` error code — deferred; needs C++ client handling first.
