# thomaz-api

Backend HTTP para o [thomaz](../README.md): autenticação (username + senha), feed da comunidade e sync de saves na nuvem (blob opaco + revisão).

Stack: **Fastify 5**, **TypeScript**, **Prisma**, **PostgreSQL**.

Contrato REST documentado em [`docs/superpowers/specs/2026-06-03-thomaz-api.md`](../docs/superpowers/specs/2026-06-03-thomaz-api.md) (espelha `IFeedClient` no cliente C++).

### Endpoints principais

| Área | Rotas |
|------|--------|
| Auth | `POST /auth/register`, `/login`, `/refresh`, `/logout` |
| Feed | `GET /feed` |
| Posts | `POST /posts`, `PUT /posts/:id/like`, `GET/POST /posts/:id/comments`, `DELETE /posts/:id` |
| Saves | `GET /saves`, `GET/PUT/DELETE /saves/:titleId` |
| Users | `GET /users/me`, `GET /users/:username`, `GET /users/:username/posts` |
| Health | `GET /health` |

## Requisitos

- Node.js 20+
- Docker (para PostgreSQL local)

## Subir localmente

```bash
cd api
docker compose up -d
cp .env.example .env
# Edite JWT_SECRET se quiser (mín. 16 caracteres)
npm install
npx prisma migrate deploy
npm run dev
```

API em `http://localhost:3000` — `GET /health` para checar.

Uploads de imagem ficam em `api/uploads/` e são servidos em `/uploads/<imageKey>`.

## Variáveis de ambiente

| Variável | Descrição |
|----------|-----------|
| `DATABASE_URL` | Connection string PostgreSQL |
| `JWT_SECRET` | Segredo para tokens (nunca commitar) |
| `PORT` | Porta HTTP (padrão 3000) |
| `PUBLIC_BASE_URL` | Base das URLs de imagem no JSON do feed |
| `UPLOAD_DIR` | Pasta local de JPEGs (padrão `./uploads`) |
| `CORS_ORIGINS` | Origens permitidas, separadas por vírgula |
| `JWT_ACCESS_EXPIRES` | Duração do access JWT (padrão `15m`) |
| `REFRESH_TOKEN_TTL_DAYS` | TTL do refresh token em dias (padrão `7`) |

## Scripts

| Comando | Uso |
|---------|-----|
| `npm run dev` | Servidor com reload (tsx) |
| `npm run build` | Compila para `dist/` |
| `npm test` | Testes de integração (Vitest) |
| `npm run db:migrate` | Nova migration em dev |
| `npm run db:deploy` | Aplica migrations (CI/prod) |

## Produção

Use PostgreSQL gerenciado (Neon, Supabase, etc.), defina `JWT_SECRET` forte e `PUBLIC_BASE_URL` com HTTPS. Troque storage local por S3/R2 quando escalar uploads.

Nunca commite `.env` nem dumps de banco.
