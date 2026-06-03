import Fastify, { type FastifyInstance } from "fastify";
import cors from "@fastify/cors";
import rateLimit from "@fastify/rate-limit";
import multipart from "@fastify/multipart";
import fastifyStatic from "@fastify/static";
import { join } from "node:path";
import { loadConfig, corsOrigins, type Env } from "./config.js";
import { registerDb } from "./plugins/db.js";
import { registerAuth } from "./plugins/auth.js";
import { ensureUploadDir } from "./lib/storage.js";
import { authRoutes } from "./routes/auth.js";
import { feedRoutes } from "./routes/feed.js";
import { postsRoutes } from "./routes/posts.js";
import { savesRoutes } from "./routes/saves.js";
import { usersRoutes } from "./routes/users.js";

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

  const app = Fastify({ logger: false });

  await registerDb(app);
  await registerAuth(app, env);

  await app.register(cors, { origin: corsOrigins(env) });
  await app.register(rateLimit, {
    max: env.NODE_ENV === "test" ? 10_000 : 100,
    timeWindow: "1 minute",
  });
  await app.register(multipart, {
    limits: { fileSize: 16 * 1024 * 1024 },
  });

  await app.register(fastifyStatic, {
    root: join(process.cwd(), env.UPLOAD_DIR),
    prefix: "/uploads/",
    decorateReply: false,
  });

  app.get("/health", async () => ({ status: "ok" }));

  await authRoutes(app, env);
  await feedRoutes(app, env);
  await postsRoutes(app, env);
  await savesRoutes(app, env);
  await usersRoutes(app, env);

  await app.ready();
  return { app, env };
}
