import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import FormData from "form-data";
import { afterAll, beforeAll, describe, expect, it } from "vitest";
import { buildApp } from "../src/app.js";

const TEST_DB =
  process.env.DATABASE_URL ??
  "postgresql://thomaz:thomaz@localhost:5433/thomaz?schema=public";

const JWT_SECRET = "test-jwt-secret-min-16-chars";
const SOURCE_ENC_KEY = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";

let uploadDir = "";
let app: Awaited<ReturnType<typeof buildApp>>["app"];

beforeAll(async () => {
  process.env.DATABASE_URL = TEST_DB;
  process.env.JWT_SECRET = JWT_SECRET;
  process.env.SOURCE_ENC_KEY = SOURCE_ENC_KEY;
  process.env.PUBLIC_BASE_URL = "http://localhost:3000";
  process.env.NODE_ENV = "test";
  // Keep the auth rate limit effectively off for the shared app so the many
  // register/login calls across tests don't trip it; the dedicated rate-limit
  // test below builds its own app with a low limit.
  process.env.AUTH_RATE_MAX = "10000";
  uploadDir = await mkdtemp(join(tmpdir(), "thomaz-api-"));
  process.env.UPLOAD_DIR = uploadDir;

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

  it("auth register + login", async () => {
    const user = `u_${Date.now()}`;
    const pass = "password1";

    const reg = await app.inject({
      method: "POST",
      url: "/auth/register",
      payload: { username: user, password: pass },
    });
    expect(reg.statusCode).toBe(200);
    const regBody = reg.json() as {
      ok: boolean;
      token: string;
      refreshToken: string;
    };
    expect(regBody.ok).toBe(true);
    expect(regBody.token.length).toBeGreaterThan(10);
    expect(regBody.refreshToken.length).toBeGreaterThan(10);

    const dup = await app.inject({
      method: "POST",
      url: "/auth/register",
      payload: { username: user, password: pass },
    });
    expect(dup.statusCode).toBe(409);

    const login = await app.inject({
      method: "POST",
      url: "/auth/login",
      payload: { username: user, password: pass },
    });
    expect(login.statusCode).toBe(200);
    const loginBody = login.json() as { token: string; refreshToken: string };
    expect(loginBody.token.length).toBeGreaterThan(10);

    const refresh = await app.inject({
      method: "POST",
      url: "/auth/refresh",
      payload: { refreshToken: loginBody.refreshToken },
    });
    expect(refresh.statusCode).toBe(200);
  });

  it("auth refresh and logout", async () => {
    const user = `refresh_${Date.now()}`;
    const pass = "password1";

    const login = await app.inject({
      method: "POST",
      url: "/auth/login",
      payload: { username: user, password: pass },
    });
    if (login.statusCode !== 200) {
      await app.inject({
        method: "POST",
        url: "/auth/register",
        payload: { username: user, password: pass },
      });
    }
    const login2 = await app.inject({
      method: "POST",
      url: "/auth/login",
      payload: { username: user, password: pass },
    });
    const { token: oldToken, refreshToken: oldRefresh } = login2.json() as {
      token: string;
      refreshToken: string;
    };

    const refreshed = await app.inject({
      method: "POST",
      url: "/auth/refresh",
      payload: { refreshToken: oldRefresh },
    });
    expect(refreshed.statusCode).toBe(200);
    const { token: newToken, refreshToken: newRefresh } = refreshed.json() as {
      token: string;
      refreshToken: string;
    };
    expect(newRefresh).not.toBe(oldRefresh);
    expect(newToken.length).toBeGreaterThan(10);

    // Verify new token is valid by checking a protected saves endpoint
    const savesCheck = await app.inject({
      method: "GET",
      url: "/saves",
      headers: { authorization: `Bearer ${newToken}` },
    });
    expect(savesCheck.statusCode).toBe(200);

    const oldRefreshAgain = await app.inject({
      method: "POST",
      url: "/auth/refresh",
      payload: { refreshToken: oldRefresh },
    });
    expect(oldRefreshAgain.statusCode).toBe(401);

    await app.inject({
      method: "POST",
      url: "/auth/logout",
      payload: { refreshToken: newRefresh },
    });

    const afterLogout = await app.inject({
      method: "POST",
      url: "/auth/refresh",
      payload: { refreshToken: newRefresh },
    });
    expect(afterLogout.statusCode).toBe(401);
  });

  it("saves list, upload, download with revision", async () => {
    const user = `save_${Date.now()}`;
    const pass = "password1";
    const titleId = "01008BB901469000";

    const reg = await app.inject({
      method: "POST",
      url: "/auth/register",
      payload: { username: user, password: pass },
    });
    const token = (reg.json() as { token: string }).token;

    const unauth = await app.inject({ method: "GET", url: "/saves" });
    expect(unauth.statusCode).toBe(401);

    const empty = await app.inject({
      method: "GET",
      url: "/saves",
      headers: { authorization: `Bearer ${token}` },
    });
    expect(empty.statusCode).toBe(200);
    expect((empty.json() as { slots: unknown[] }).slots).toEqual([]);

    const missing = await app.inject({
      method: "GET",
      url: `/saves/${titleId}`,
      headers: { authorization: `Bearer ${token}` },
    });
    expect(missing.statusCode).toBe(404);

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
      headers: {
        authorization: `Bearer ${token}`,
        ...form.getHeaders(),
      },
      payload: form,
    });
    expect(put.statusCode).toBe(200);
    const putBody = put.json() as {
      ok: boolean;
      slot: { revision: number; label: string; hasData: boolean };
    };
    expect(putBody.ok).toBe(true);
    expect(putBody.slot.revision).toBe(1);
    expect(putBody.slot.label).toBe("slot-a");
    expect(putBody.slot.hasData).toBe(true);

    const list = await app.inject({
      method: "GET",
      url: "/saves",
      headers: { authorization: `Bearer ${token}` },
    });
    expect(list.statusCode).toBe(200);
    const slots = (list.json() as { slots: { titleId: string }[] }).slots;
    expect(slots.some((s) => s.titleId === titleId)).toBe(true);

    const meta = await app.inject({
      method: "GET",
      url: `/saves/${titleId}`,
      headers: { authorization: `Bearer ${token}` },
    });
    expect(meta.statusCode).toBe(200);
    expect((meta.json() as { slot: { hasData: boolean } }).slot.hasData).toBe(
      true,
    );

    const withData = await app.inject({
      method: "GET",
      url: `/saves/${titleId}?includeData=1`,
      headers: { authorization: `Bearer ${token}` },
    });
    expect(withData.statusCode).toBe(200);
    const downloaded = withData.json() as { slot: { data: string } };
    expect(Buffer.from(downloaded.slot.data, "base64").equals(blob)).toBe(true);

    const blob2 = Buffer.from("encrypted-save-blob-v2");
    const form2 = new FormData();
    form2.append("data", blob2, {
      filename: "save.bin",
      contentType: "application/octet-stream",
    });
    form2.append("revision", "1");

    const put2 = await app.inject({
      method: "PUT",
      url: `/saves/${titleId}`,
      headers: {
        authorization: `Bearer ${token}`,
        ...form2.getHeaders(),
      },
      payload: form2,
    });
    expect(put2.statusCode).toBe(200);
    expect((put2.json() as { slot: { revision: number } }).slot.revision).toBe(2);

    const formConflict = new FormData();
    formConflict.append("data", blob2, {
      filename: "save.bin",
      contentType: "application/octet-stream",
    });
    formConflict.append("revision", "1");

    const conflict = await app.inject({
      method: "PUT",
      url: `/saves/${titleId}`,
      headers: {
        authorization: `Bearer ${token}`,
        ...formConflict.getHeaders(),
      },
      payload: formConflict,
    });
    expect(conflict.statusCode).toBe(409);
    expect(conflict.json()).toEqual({
      ok: false,
      error: "revision_conflict",
    });

    const del = await app.inject({
      method: "DELETE",
      url: `/saves/${titleId}`,
      headers: { authorization: `Bearer ${token}` },
    });
    expect(del.statusCode).toBe(200);

    const gone = await app.inject({
      method: "GET",
      url: `/saves/${titleId}`,
      headers: { authorization: `Bearer ${token}` },
    });
    expect(gone.statusCode).toBe(404);
  });

  it("rate-limits repeated /auth/login from the same IP", async () => {
    const prev = process.env.AUTH_RATE_MAX;
    const built = await buildApp({ AUTH_RATE_MAX: 3 });
    const rlApp = built.app;
    try {
      const hit = () =>
        rlApp.inject({
          method: "POST",
          url: "/auth/login",
          payload: { username: "ghost_user", password: "password1" },
        });

      // Under the limit (3/min): non-existent user → 401, never 429.
      const first = [await hit(), await hit(), await hit()];
      expect(first.map((r) => r.statusCode)).toEqual([401, 401, 401]);

      // The 4th request in the window is throttled.
      const throttled = await hit();
      expect(throttled.statusCode).toBe(429);
    } finally {
      await rlApp.close();
      process.env.AUTH_RATE_MAX = prev;
    }
  });

  it("rate-limits repeated /auth/register from the same IP", async () => {
    const prev = process.env.AUTH_RATE_MAX;
    const built = await buildApp({ AUTH_RATE_MAX: 2 });
    const rlApp = built.app;
    try {
      const hit = (n: number) =>
        rlApp.inject({
          method: "POST",
          url: "/auth/register",
          payload: { username: `rl_reg_${Date.now()}_${n}`, password: "password1" },
        });

      const r1 = await hit(1);
      const r2 = await hit(2);
      const r3 = await hit(3);
      expect(r1.statusCode).toBe(200);
      expect(r2.statusCode).toBe(200);
      expect(r3.statusCode).toBe(429);
    } finally {
      await rlApp.close();
      process.env.AUTH_RATE_MAX = prev;
    }
  });

  // ── SEC-02: jti minting ───────────────────────────────────────────────────
  // Helper: decode a JWT payload without verifying the signature
  function decodeJwtPayload(token: string): Record<string, unknown> {
    const seg = token.split(".")[1];
    return JSON.parse(Buffer.from(seg, "base64url").toString("utf8")) as Record<string, unknown>;
  }

  it("SEC-02 T1: access token from register carries a non-empty jti and exp", async () => {
    const user = `jti_reg_${Date.now()}`;
    const reg = await app.inject({
      method: "POST",
      url: "/auth/register",
      payload: { username: user, password: "password1" },
    });
    expect(reg.statusCode).toBe(200);
    const { token } = reg.json() as { token: string };
    const payload = decodeJwtPayload(token);
    expect(typeof payload.jti).toBe("string");
    expect((payload.jti as string).length).toBeGreaterThan(0);
    expect(typeof payload.exp).toBe("number");
  });

  it("SEC-02 T1: access token from /auth/refresh carries a non-empty jti", async () => {
    const user = `jti_refresh_${Date.now()}`;
    const reg = await app.inject({
      method: "POST",
      url: "/auth/register",
      payload: { username: user, password: "password1" },
    });
    const { refreshToken } = reg.json() as { refreshToken: string };
    const refreshed = await app.inject({
      method: "POST",
      url: "/auth/refresh",
      payload: { refreshToken },
    });
    expect(refreshed.statusCode).toBe(200);
    const { token } = refreshed.json() as { token: string };
    const payload = decodeJwtPayload(token);
    expect(typeof payload.jti).toBe("string");
    expect((payload.jti as string).length).toBeGreaterThan(0);
  });

  it("SEC-02 T1: two separately minted access tokens have different jti values", async () => {
    const user = `jti_uniq_${Date.now()}`;
    const r1 = await app.inject({
      method: "POST",
      url: "/auth/register",
      payload: { username: user, password: "password1" },
    });
    const r2 = await app.inject({
      method: "POST",
      url: "/auth/login",
      payload: { username: user, password: "password1" },
    });
    const p1 = decodeJwtPayload((r1.json() as { token: string }).token);
    const p2 = decodeJwtPayload((r2.json() as { token: string }).token);
    expect(p1.jti).not.toBe(p2.jti);
  });

  // ── SEC-02: blocklist enforcement ─────────────────────────────────────────
  it("SEC-02 T2: logged-out access token is rejected; sibling token still works", async () => {
    const user = `revoke_${Date.now()}`;
    const pass = "password1";

    // Register (gives tokenA1 + refreshA1)
    const reg = await app.inject({
      method: "POST",
      url: "/auth/register",
      payload: { username: user, password: pass },
    });
    const { token: tokenA1, refreshToken: refreshA1 } = reg.json() as {
      token: string;
      refreshToken: string;
    };

    // Login again (gives tokenA2 + refreshA2) — kept valid
    const login = await app.inject({
      method: "POST",
      url: "/auth/login",
      payload: { username: user, password: pass },
    });
    const { token: tokenA2, refreshToken: refreshA2 } = login.json() as {
      token: string;
      refreshToken: string;
    };

    // Logout with tokenA1 in Authorization header + refreshA1 in body
    const logout = await app.inject({
      method: "POST",
      url: "/auth/logout",
      headers: { authorization: `Bearer ${tokenA1}` },
      payload: { refreshToken: refreshA1 },
    });
    expect(logout.statusCode).toBe(200);
    expect(logout.json()).toEqual({ ok: true });

    // tokenA1 → 401 (revoked)
    const withRevoked = await app.inject({
      method: "GET",
      url: "/saves",
      headers: { authorization: `Bearer ${tokenA1}` },
    });
    expect(withRevoked.statusCode).toBe(401);
    expect(withRevoked.json()).toEqual({ ok: false, error: "unauthorized" });

    // tokenA2 → 200 (not revoked)
    const withValid = await app.inject({
      method: "GET",
      url: "/saves",
      headers: { authorization: `Bearer ${tokenA2}` },
    });
    expect(withValid.statusCode).toBe(200);

    // cleanup: logout tokenA2
    await app.inject({
      method: "POST",
      url: "/auth/logout",
      headers: { authorization: `Bearer ${tokenA2}` },
      payload: { refreshToken: refreshA2 },
    });
  });

  it("SEC-02 T2: POST /auth/logout with no bearer still returns 200", async () => {
    const user = `logout_nobearer_${Date.now()}`;
    const reg = await app.inject({
      method: "POST",
      url: "/auth/register",
      payload: { username: user, password: "password1" },
    });
    const { refreshToken } = reg.json() as { refreshToken: string };

    const logout = await app.inject({
      method: "POST",
      url: "/auth/logout",
      payload: { refreshToken },
    });
    expect(logout.statusCode).toBe(200);
    expect(logout.json()).toEqual({ ok: true });
  });

  // ── TEST-01: SEC-01 regression guard ─────────────────────────────────────
  it("TEST-01: direct save-blob path is not publicly accessible", async () => {
    const res = await app.inject({
      method: "GET",
      url: "/uploads/saves/some-user-id/01008BB901469000.bin",
    });
    expect(res.statusCode).toBe(404); // no static route serves uploads/
  });

  // ── TEST-02: revision_required branch (fourth PUT revision matrix branch) ─
  it("TEST-02: PUT to existing slot without revision → 400 revision_required", async () => {
    const user = `rev_req_${Date.now()}`;
    const pass = "password1";
    const titleId = `010000${Date.now().toString(16).toUpperCase().padStart(10, "0")}`;

    const reg = await app.inject({
      method: "POST",
      url: "/auth/register",
      payload: { username: user, password: pass },
    });
    const token = (reg.json() as { token: string }).token;

    // First PUT: create the slot (new-slot 200, revision 1)
    const blob1 = Buffer.from("save-data-v1");
    const form1 = new FormData();
    form1.append("data", blob1, {
      filename: "save.bin",
      contentType: "application/octet-stream",
    });
    const create = await app.inject({
      method: "PUT",
      url: `/saves/${titleId}`,
      headers: { authorization: `Bearer ${token}`, ...form1.getHeaders() },
      payload: form1,
    });
    expect(create.statusCode).toBe(200);
    expect((create.json() as { slot: { revision: number } }).slot.revision).toBe(1);

    // Second PUT: update existing slot WITHOUT a revision field → 400 revision_required
    const blob2 = Buffer.from("save-data-v2");
    const form2 = new FormData();
    form2.append("data", blob2, {
      filename: "save.bin",
      contentType: "application/octet-stream",
    });
    // intentionally omit form2.append("revision", ...) to hit the revision_required branch
    const noRevision = await app.inject({
      method: "PUT",
      url: `/saves/${titleId}`,
      headers: { authorization: `Bearer ${token}`, ...form2.getHeaders() },
      payload: form2,
    });
    expect(noRevision.statusCode).toBe(400);
    expect(noRevision.json()).toEqual({ ok: false, error: "revision_required" });
  });

  it("SEC-02 T2: double logout with same token does not crash (200 both times)", async () => {
    const user = `dbllogout_${Date.now()}`;
    const reg = await app.inject({
      method: "POST",
      url: "/auth/register",
      payload: { username: user, password: "password1" },
    });
    const { token, refreshToken } = reg.json() as {
      token: string;
      refreshToken: string;
    };
    // Extra refresh token to have something valid for second logout body
    const login = await app.inject({
      method: "POST",
      url: "/auth/login",
      payload: { username: user, password: "password1" },
    });
    const { refreshToken: refreshToken2 } = login.json() as {
      refreshToken: string;
    };

    const first = await app.inject({
      method: "POST",
      url: "/auth/logout",
      headers: { authorization: `Bearer ${token}` },
      payload: { refreshToken },
    });
    expect(first.statusCode).toBe(200);

    const second = await app.inject({
      method: "POST",
      url: "/auth/logout",
      headers: { authorization: `Bearer ${token}` },
      payload: { refreshToken: refreshToken2 },
    });
    expect(second.statusCode).toBe(200);
    expect(second.json()).toEqual({ ok: true });
  });
});
