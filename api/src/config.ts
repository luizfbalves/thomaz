import { z } from "zod";

const envSchema = z.object({
  DATABASE_URL: z.string().min(1),
  JWT_SECRET: z.string().min(16),
  PORT: z.coerce.number().int().positive().default(3000),
  HOST: z.string().default("0.0.0.0"),
  PUBLIC_BASE_URL: z.string().url(),
  UPLOAD_DIR: z.string().default("./uploads"),
  CORS_ORIGINS: z.string().default("http://localhost:3000"),
  NODE_ENV: z
    .enum(["development", "production", "test"])
    .default("development"),
  // Long-lived sessions: the homebrew client stores the access token and does
  // not auto-refresh for cloud saves, so a short access token surfaces as
  // "session expired". Default to 1 year for a frictionless console experience.
  JWT_ACCESS_EXPIRES: z.string().default("365d"),
  REFRESH_TOKEN_TTL_DAYS: z.coerce.number().int().positive().default(365),
  // Max requests/minute per IP for the credential endpoints (/auth/login,
  // /auth/register). Strict by default to blunt brute-force and signup spam;
  // raise via env if legitimate users behind shared NAT hit it.
  AUTH_RATE_MAX: z.coerce.number().int().positive().default(10),
});

export type Env = z.infer<typeof envSchema>;

export function loadConfig(): Env {
  const parsed = envSchema.safeParse(process.env);
  if (!parsed.success) {
    console.error("Invalid environment:", parsed.error.flatten().fieldErrors);
    throw new Error("Invalid environment configuration");
  }
  return parsed.data;
}

export function corsOrigins(env: Env): string[] {
  return env.CORS_ORIGINS.split(",")
    .map((s) => s.trim())
    .filter(Boolean);
}
