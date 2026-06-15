#!/bin/sh
set -e

# Sync schema to the database. We use `db push` (no migrations committed in
# this scaffold). If you later add migrations under prisma/migrations, switch
# this to `prisma migrate deploy`.
echo "Syncing schema with prisma db push..."
npx prisma db push --skip-generate --accept-data-loss

echo "Starting API..."
exec node dist/index.js
