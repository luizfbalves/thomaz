#!/usr/bin/env bash
# Build + migrate + (re)start thomaz-api under PM2. Run from the api/ directory on the VM.
# Non-disruptive: does NOT touch n8n / zapi / loop / postgres.
set -euo pipefail

# Node/PM2 are not on the non-interactive PATH; make them available.
export PATH="/opt/bitnami/node/bin:$HOME/.npm-global/bin:$PATH"
[ -s "$HOME/.nvm/nvm.sh" ] && . "$HOME/.nvm/nvm.sh"

log() { printf '\n[%s] %s\n' "$(date -Iseconds)" "$*"; }

log "Installing dependencies (npm ci)..."
npm ci

log "Generating Prisma client..."
npx prisma generate

log "Applying database migrations..."
npx prisma migrate deploy

log "Building with SWC..."
npm run build

log "Reloading PM2 process..."
if pm2 describe thomaz-api > /dev/null 2>&1; then
  pm2 reload thomaz-api --update-env
else
  pm2 start ecosystem.config.cjs
fi
pm2 save

log "Done."
pm2 describe thomaz-api | grep -E "status|restarts" || true
