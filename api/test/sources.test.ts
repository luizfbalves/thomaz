import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { afterAll, beforeAll, describe, expect, it } from "vitest";
import { buildApp } from "../src/app.js";
import { PrismaClient } from "@prisma/client";

const TEST_DB =
  process.env.DATABASE_URL ??
  "postgresql://thomaz:thomaz@localhost:5433/thomaz?schema=public";

const JWT_SECRET = "test-jwt-secret-min-16-chars";
const SOURCE_ENC_KEY = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";

let uploadDir = "";
let app: Awaited<ReturnType<typeof buildApp>>["app"];
let prisma: PrismaClient;

async function registerUser(username: string): Promise<string> {
  const reg = await app.inject({
    method: "POST",
    url: "/auth/register",
    payload: { username, password: "password1" },
  });
  expect(reg.statusCode).toBe(200);
  return (reg.json() as { token: string }).token;
}

beforeAll(async () => {
  process.env.DATABASE_URL = TEST_DB;
  process.env.JWT_SECRET = JWT_SECRET;
  process.env.SOURCE_ENC_KEY = SOURCE_ENC_KEY;
  process.env.PUBLIC_BASE_URL = "http://localhost:3000";
  process.env.NODE_ENV = "test";
  process.env.AUTH_RATE_MAX = "10000";
  uploadDir = await mkdtemp(join(tmpdir(), "thomaz-sources-"));
  process.env.UPLOAD_DIR = uploadDir;

  const built = await buildApp();
  app = built.app;
  prisma = new PrismaClient({ datasources: { db: { url: TEST_DB } } });
});

afterAll(async () => {
  await app.close();
  await prisma.$disconnect();
  await rm(uploadDir, { recursive: true, force: true });
});

describe("sources", () => {
  it("GET /sources without auth returns 401", async () => {
    const res = await app.inject({ method: "GET", url: "/sources" });
    expect(res.statusCode).toBe(401);
  });

  it("owner can PUT then GET source without leaking secret", async () => {
    const user = `src_own_${Date.now()}`;
    const token = await registerUser(user);
    const sourceId = `clsrc${Date.now()}`;
    const secret = "my-header-token-secret";

    const put = await app.inject({
      method: "PUT",
      url: `/sources/${sourceId}`,
      headers: { authorization: `Bearer ${token}` },
      payload: {
        label: "My Shop",
        url: "https://example.com/tinfoil",
        authType: "header",
        secret,
      },
    });
    expect(put.statusCode).toBe(200);
    const putBody = put.json() as {
      ok: boolean;
      source: {
        id: string;
        label: string;
        url: string;
        authType: string;
        hasSecret: boolean;
      };
    };
    expect(putBody.ok).toBe(true);
    expect(putBody.source.id).toBe(sourceId);
    expect(putBody.source.label).toBe("My Shop");
    expect(putBody.source.authType).toBe("header");
    expect(putBody.source.hasSecret).toBe(true);
    expect(JSON.stringify(putBody)).not.toContain(secret);

    const get = await app.inject({
      method: "GET",
      url: `/sources/${sourceId}`,
      headers: { authorization: `Bearer ${token}` },
    });
    expect(get.statusCode).toBe(200);
    const getBody = get.json() as { source: { hasSecret: boolean } };
    expect(getBody.source.hasSecret).toBe(true);
    expect(get.body).not.toContain(secret);

    const row = await prisma.sourceLink.findFirst({
      where: { id: sourceId },
    });
    expect(row?.authSecretEnc).toBeTruthy();
    expect(row?.authSecretEnc).not.toBe(secret);
  });

  it("second user cannot read first user's source", async () => {
    const userA = `src_a_${Date.now()}`;
    const userB = `src_b_${Date.now()}`;
    const tokenA = await registerUser(userA);
    const tokenB = await registerUser(userB);
    const sourceId = `clsrc${Date.now()}_iso`;

    await app.inject({
      method: "PUT",
      url: `/sources/${sourceId}`,
      headers: { authorization: `Bearer ${tokenA}` },
      payload: {
        label: "Private",
        url: "https://private.example/index",
        authType: "none",
      },
    });

    const crossGet = await app.inject({
      method: "GET",
      url: `/sources/${sourceId}`,
      headers: { authorization: `Bearer ${tokenB}` },
    });
    expect(crossGet.statusCode).toBe(404);

    const listB = await app.inject({
      method: "GET",
      url: "/sources",
      headers: { authorization: `Bearer ${tokenB}` },
    });
    expect(listB.statusCode).toBe(200);
    const sources = (listB.json() as { sources: { id: string }[] }).sources;
    expect(sources.some((s) => s.id === sourceId)).toBe(false);
  });
});
