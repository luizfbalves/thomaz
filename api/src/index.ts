import { loadConfig } from "./config.js";
import { buildApp } from "./app.js";

async function main() {
  const { app, env } = await buildApp();
  await app.listen({ port: env.PORT, host: env.HOST });
  console.log(`thomaz-api listening on ${env.HOST}:${env.PORT}`);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});

export { main };
