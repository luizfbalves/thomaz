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

let uploadDir = "";
let app: Awaited<ReturnType<typeof buildApp>>["app"];

beforeAll(async () => {
  process.env.DATABASE_URL = TEST_DB;
  process.env.JWT_SECRET = JWT_SECRET;
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

  it("GET /feed rejects invalid cursor", async () => {
    const res = await app.inject({
      method: "GET",
      url: "/feed?cursor=not-a-valid-cursor",
    });
    expect(res.statusCode).toBe(400);
    expect(res.json()).toEqual({ ok: false, error: "invalid_cursor" });
  });

  it("auth register + login + feed flow", async () => {
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
    const token = loginBody.token;

    const jpeg = Buffer.from([0xff, 0xd8, 0xff, 0xd9]);
    const form = new FormData();
    form.append("caption", "hello feed");
    form.append("gameTitleId", "12345");
    form.append("gameName", "Test Game");
    form.append("image", jpeg, {
      filename: "shot.jpg",
      contentType: "image/jpeg",
    });

    const post = await app.inject({
      method: "POST",
      url: "/posts",
      headers: {
        authorization: `Bearer ${token}`,
        ...form.getHeaders(),
      },
      payload: form,
    });
    expect(post.statusCode).toBe(200);
    const postBody = post.json() as {
      ok: boolean;
      post: { id: string; caption: string; gameTitleId: string };
    };
    expect(postBody.ok).toBe(true);
    expect(postBody.post.caption).toBe("hello feed");
    expect(postBody.post.gameTitleId).toBe("0000000000003039");

    const feed = await app.inject({
      method: "GET",
      url: "/feed",
      headers: { authorization: `Bearer ${token}` },
    });
    expect(feed.statusCode).toBe(200);
    const feedBody = feed.json() as {
      posts: { id: string }[];
      hasMore: boolean;
    };
    expect(feedBody.posts.some((p) => p.id === postBody.post.id)).toBe(true);

    const like = await app.inject({
      method: "PUT",
      url: `/posts/${postBody.post.id}/like`,
      headers: { authorization: `Bearer ${token}` },
      payload: { liked: true },
    });
    expect(like.statusCode).toBe(200);

    const comment = await app.inject({
      method: "POST",
      url: `/posts/${postBody.post.id}/comments`,
      headers: { authorization: `Bearer ${token}` },
      payload: { text: "nice" },
    });
    expect(comment.statusCode).toBe(200);

    const comments = await app.inject({
      method: "GET",
      url: `/posts/${postBody.post.id}/comments`,
    });
    expect(comments.statusCode).toBe(200);
    const list = comments.json() as { text: string }[];
    expect(list.some((c) => c.text === "nice")).toBe(true);

    const me = await app.inject({
      method: "GET",
      url: "/users/me",
      headers: { authorization: `Bearer ${token}` },
    });
    expect(me.statusCode).toBe(200);
    const meBody = me.json() as { username: string; postCount: number };
    expect(meBody.username).toBe(user);
    expect(meBody.postCount).toBe(1);

    const profile = await app.inject({
      method: "GET",
      url: `/users/${user}`,
    });
    expect(profile.statusCode).toBe(200);
    expect((profile.json() as { postCount: number }).postCount).toBe(1);

    const userPosts = await app.inject({
      method: "GET",
      url: `/users/${user}/posts`,
    });
    expect(userPosts.statusCode).toBe(200);
    expect(
      (userPosts.json() as { posts: { id: string }[] }).posts.some(
        (p) => p.id === postBody.post.id,
      ),
    ).toBe(true);

    const del = await app.inject({
      method: "DELETE",
      url: `/posts/${postBody.post.id}`,
      headers: { authorization: `Bearer ${token}` },
    });
    expect(del.statusCode).toBe(200);

    const gone = await app.inject({
      method: "GET",
      url: `/posts/${postBody.post.id}/comments`,
    });
    expect(gone.statusCode).toBe(404);

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

    const me = await app.inject({
      method: "GET",
      url: "/users/me",
      headers: { authorization: `Bearer ${newToken}` },
    });
    expect(me.statusCode).toBe(200);

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

  it("DELETE post forbidden for non-author", async () => {
    const author = `author_${Date.now()}`;
    const other = `other_${Date.now()}`;
    const pass = "password1";

    for (const u of [author, other]) {
      await app.inject({
        method: "POST",
        url: "/auth/register",
        payload: { username: u, password: pass },
      });
    }

    const authorToken = (
      await app.inject({
        method: "POST",
        url: "/auth/login",
        payload: { username: author, password: pass },
      })
    ).json() as { token: string };

    const otherToken = (
      await app.inject({
        method: "POST",
        url: "/auth/login",
        payload: { username: other, password: pass },
      })
    ).json() as { token: string };

    const form = new FormData();
    form.append("image", Buffer.from([0xff, 0xd8, 0xff, 0xd9]), {
      filename: "shot.jpg",
      contentType: "image/jpeg",
    });

    const post = await app.inject({
      method: "POST",
      url: "/posts",
      headers: {
        authorization: `Bearer ${authorToken.token}`,
        ...form.getHeaders(),
      },
      payload: form,
    });
    const postId = (post.json() as { post: { id: string } }).post.id;

    const forbidden = await app.inject({
      method: "DELETE",
      url: `/posts/${postId}`,
      headers: { authorization: `Bearer ${otherToken.token}` },
    });
    expect(forbidden.statusCode).toBe(403);
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
});
