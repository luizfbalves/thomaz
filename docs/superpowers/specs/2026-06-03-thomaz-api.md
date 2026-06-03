# Spec — thomaz API (HTTP)

**Data:** 2026-06-03  
**Status:** v1 + v2 (perfil, delete, refresh) implementada em `api/`  
**Cliente:** `IFeedClient` / futuro `HttpFeedClient` — ver [community-feed-design](2026-06-03-community-feed-design.md)

---

## 1. Base URL

- Desenvolvimento: `http://localhost:3000`
- Produção: configurável no app (`API_BASE_URL`); HTTPS obrigatório para auth e saves.

---

## 2. Autenticação

Registro e login retornam JWT Bearer. O cliente guarda o token em `Session` (não persiste senha).

Header nas rotas protegidas:

```
Authorization: Bearer <token>
```

### POST `/auth/register`

Body JSON:

```json
{ "username": "player1", "password": "secret12" }
```

- `username`: 3–32 chars, `[a-zA-Z0-9_]`
- `password`: 6–128 chars

Resposta 200:

```json
{ "ok": true, "token": "<access-jwt>", "refreshToken": "<opaque>" }
```

- `token`: JWT de acesso (expira em `JWT_ACCESS_EXPIRES`, padrão 15m).
- `refreshToken`: opaco, válido por `REFRESH_TOKEN_TTL_DAYS` (padrão 7).

Erros: `409` `{ "ok": false, "error": "username_already_exists" }`

### POST `/auth/login`

Mesmo body. Resposta 200 igual. Erro `401` `{ "ok": false, "error": "invalid_credentials" }`

### POST `/auth/refresh`

Body: `{ "refreshToken": "..." }`

Resposta 200: novo par `{ "ok": true, "token", "refreshToken" }` (rotação: o refresh antigo deixa de valer).

Erro `401` `invalid_refresh_token`.

### POST `/auth/logout`

Body: `{ "refreshToken": "..." }`. Revoga o refresh. Resposta 200: `{ "ok": true }`.

---

## 3. Feed

### GET `/feed?cursor=`

Leitura pública. `cursor` opcional (opaco, valor de `nextCursor` da página anterior).

Resposta 200 — alinhada a `feed::FeedPage`:

```json
{
  "posts": [
    {
      "id": "clx...",
      "author": { "id": "...", "username": "player1" },
      "imageUrl": "http://localhost:3000/uploads/uuid.jpg",
      "caption": "boss final",
      "gameTitleId": "01008BB901469000",
      "gameName": "Zelda",
      "likeCount": 3,
      "likedByMe": false,
      "commentCount": 1,
      "createdAt": 1717430400
    }
  ],
  "nextCursor": "base64url...",
  "hasMore": true
}
```

Com `Authorization` opcional: `likedByMe` reflete o usuário autenticado.

Erro `400` `invalid_cursor` se `cursor` for inválido.

---

## 4. Posts

### POST `/posts`

**Auth obrigatória.** `multipart/form-data`:

| Campo | Tipo | Obrigatório |
|-------|------|-------------|
| `image` | file JPEG | sim |
| `caption` | string | não |
| `gameTitleId` | string (hex 16 nybbles ou decimal u64) | não |
| `gameName` | string | não |

Limite: 5 MB, `image/jpeg`.

Resposta 200: `{ "ok": true, "post": { ... } }` (mesmo shape de item do feed).

Erros: `413` image_too_large, `400` missing_image / invalid_image_type.

### PUT `/posts/:postId/like`

**Auth obrigatória.** Body: `{ "liked": true }` ou `{ "liked": false }`

Resposta 200: `{ "ok": true }`

### GET `/posts/:postId/comments`

Público. Array de `Comment`:

```json
[
  {
    "id": "...",
    "author": { "id": "...", "username": "..." },
    "text": "nice shot",
    "createdAt": 1717430500
  }
]
```

### POST `/posts/:postId/comments`

**Auth obrigatória.** Body: `{ "text": "..." }` (1–2000 chars).

Resposta 200: `{ "ok": true }`

### DELETE `/posts/:postId`

**Auth obrigatória.** Apenas o autor. Remove post, likes, comentários e o JPEG.

Resposta 200: `{ "ok": true }`. Erros: `401`, `403 forbidden`, `404 post_not_found`.

---

## 5. Saves (sync na nuvem)

**Auth obrigatória** em todas as rotas. `titleId` na URL é o application id do jogo: **16 nybbles hex** (ex. `01008BB901469000`) ou decimal u64.

Blobs são **opacos** no servidor (criptografia E2E no cliente). Limite: **16 MB** por upload. Controle de concorrência via campo `revision`.

### GET `/saves`

Lista os slots do usuário autenticado.

Resposta 200:

```json
{
  "slots": [
    {
      "titleId": "01008BB901469000",
      "label": "slot-a",
      "revision": 2,
      "updatedAt": 1717430400,
      "hasData": true
    }
  ]
}
```

### GET `/saves/:titleId`

Metadados de um slot. Query opcional `includeData=1` inclui o blob em base64:

```json
{
  "slot": {
    "titleId": "01008BB901469000",
    "label": "slot-a",
    "revision": 2,
    "updatedAt": 1717430400,
    "hasData": true,
    "data": "<base64>"
  }
}
```

Erros: `404` save_not_found / save_data_missing, `400` invalid_title_id.

### PUT `/saves/:titleId`

**multipart/form-data:**

| Campo | Tipo | Obrigatório |
|-------|------|-------------|
| `data` | arquivo (octet-stream) | sim |
| `label` | string (≤64) | não |
| `revision` | número | sim se o slot já existe |

- Primeiro upload: omita `revision` ou envie `0`.
- Atualização: envie a `revision` atual; o servidor incrementa para `revision + 1`.
- Conflito: `409` `revision_conflict` se `revision` não bater.

Resposta 200: `{ "ok": true, "slot": { ... } }`

Erros: `413` save_too_large, `400` missing_save_data / revision_required / invalid_body.

Armazenamento: `SaveSlot` + arquivo em `UPLOAD_DIR/saves/<userId>/<titleId>.bin` (não exposto em URL pública).

### DELETE `/saves/:titleId`

**Auth obrigatória.** Remove slot e blob. Resposta 200: `{ "ok": true }`.

Erros: `401`, `400 invalid_title_id`, `404 save_not_found`.

Fora do escopo: moderação de blobs, zip, múltiplos slots por jogo (1 slot por `userId`+`titleId`).

---

## 6. Usuários (perfil)

### GET `/users/me`

**Auth obrigatória.**

```json
{ "id": "...", "username": "player1", "createdAt": 1717430400, "postCount": 3 }
```

### GET `/users/:username`

Público. Mesmo shape (sem dados sensíveis). `404 user_not_found`.

### GET `/users/:username/posts?cursor=`

Público. Mesmo shape de `GET /feed`, filtrado pelo autor. `404 user_not_found`.

---

## 7. Mapeamento `IFeedClient` ↔ HTTP

| `IFeedClient` | HTTP |
|---------------|------|
| `registerUser(user, pass)` | `POST /auth/register` |
| `login(user, pass)` | `POST /auth/login` |
| `fetchFeed(cursor)` | `GET /feed?cursor=` |
| `createPost(token, jpeg, caption, gameTitleId, gameName)` | `POST /posts` multipart |
| `setLike(token, postId, liked)` | `PUT /posts/:id/like` |
| `fetchComments(postId)` | `GET /posts/:id/comments` |
| `addComment(token, postId, text)` | `POST /posts/:id/comments` |

---

## 8. Outros endpoints

| Método | Rota | Descrição |
|--------|------|-----------|
| GET | `/health` | `{ "status": "ok" }` |
| GET | `/uploads/:file` | Arquivo JPEG estático |

---

## 9. Segurança

- Senhas: Argon2id no servidor
- Access JWT curto + refresh opaco com hash SHA-256 no banco (rotação em `/auth/refresh`)
- Rate limit global (100 req/min; relaxado em `NODE_ENV=test`)
- CORS: origens em `CORS_ORIGINS`
- Segredos só via ambiente — nunca no repositório

Variáveis: `JWT_ACCESS_EXPIRES` (padrão `15m`), `REFRESH_TOKEN_TTL_DAYS` (padrão `7`).

Fora do escopo: moderação, E2E inspeção de blobs no servidor.
