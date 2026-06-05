import Fastify, { type FastifyInstance } from "fastify";
import cors from "@fastify/cors";
import rateLimit from "@fastify/rate-limit";
import multipart from "@fastify/multipart";
import { loadConfig, corsOrigins, type Env } from "./config.js";
import { registerDb } from "./plugins/db.js";
import { registerAuth } from "./plugins/auth.js";
import { ensureUploadDir } from "./lib/storage.js";
import { authRoutes } from "./routes/auth.js";
import { savesRoutes } from "./routes/saves.js";

export async function buildApp(
  overrides?: Partial<Env>,
): Promise<{ app: FastifyInstance; env: Env }> {
  if (overrides) {
    for (const [k, v] of Object.entries(overrides)) {
      if (v !== undefined) process.env[k] = String(v);
    }
  }

  const env = loadConfig();
  await ensureUploadDir(env);

  const redact = {
    paths: ["req.headers.authorization", "req.headers.cookie"],
    remove: true,
  };
  const envToLogger = {
    development: {
      redact,
      transport: {
        target: "pino-pretty",
        options: { translateTime: "HH:MM:ss Z", ignore: "pid,hostname" },
      },
    },
    production: { redact }, // object (NOT `true`) so redact applies — Pitfall 2 / D-11
    test: false, // D-10: keeps Vitest suite silent
  } as const;

  const app = Fastify({ logger: envToLogger[env.NODE_ENV] });

  await registerDb(app);
  await registerAuth(app, env);

  await app.register(cors, { origin: corsOrigins(env) });
  await app.register(rateLimit, {
    max: env.NODE_ENV === "test" ? 10_000 : 100,
    timeWindow: "1 minute",
  });
  // @fastify/multipart is retained for save blob uploads (PUT /saves/:titleId).
  // Community image upload was removed with posts.ts; save blobs are binary
  // application/octet-stream with a tighter 4 MB limit (Switch save files).
  await app.register(multipart, {
    limits: { fileSize: 4 * 1024 * 1024 },
  });

  app.get("/health", async () => ({ status: "ok" }));

  await authRoutes(app, env);
  await savesRoutes(app, env);

  await app.ready();
  return { app, env };
}
