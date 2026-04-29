#!/usr/bin/env bash
# dpowcoin-snapshot.sh — публикация архива chainstate+blocks для быстрого
# старта новых нод (см. .lumen/docs/13-performance-and-ux-roadmap.md §A1).
#
# Запускать на серверной ноде, которая уже синхронизирована до tip.
# Ожидаемая инфра: rclone-remote (Cloudflare R2 / S3 / B2) и nginx-фронт.
#
# Usage:
#   ./dpowcoin-snapshot.sh                       # mainnet, дефолтные пути
#   DATA_DIR=/var/lib/dpowcoin ./dpowcoin-snapshot.sh
#   REMOTE=r2:dpowcoin-snapshots ./dpowcoin-snapshot.sh
#
# Crontab (ежедневно в 03:30 UTC):
#   30 3 * * * /opt/dpowcoin/contrib/dpowcoin-snapshot.sh >> /var/log/dpowcoin-snapshot.log 2>&1

set -euo pipefail

DATA_DIR="${DATA_DIR:-$HOME/.dpowcoin}"
CLI="${CLI:-dpowcoin-cli}"
WORK_DIR="${WORK_DIR:-/tmp/dpowcoin-snapshot}"
REMOTE="${REMOTE:-r2:dpowcoin-snapshots}"          # rclone remote
PUBLIC_URL="${PUBLIC_URL:-https://snapshot.dpowcore.org}"
GPG_KEY="${GPG_KEY:-}"                              # необязательно: fingerprint maintainer-а
KEEP_LOCAL="${KEEP_LOCAL:-2}"                       # сколько последних архивов держать локально

log()  { printf '[%s] %s\n' "$(date -Iseconds)" "$*"; }
fail() { log "ERROR: $*"; exit 1; }

command -v "$CLI"   >/dev/null || fail "$CLI не найден в PATH"
command -v zstd     >/dev/null || fail "zstd не установлен (apt install zstd)"
command -v rclone   >/dev/null || fail "rclone не установлен"
command -v sha256sum >/dev/null || fail "coreutils sha256sum нужен"

# 1. Узнать tip и убедиться, что нода в синхроне.
INFO="$("$CLI" -datadir="$DATA_DIR" getblockchaininfo)"
HEIGHT=$(echo "$INFO" | grep -oP '"blocks"\s*:\s*\K[0-9]+')
PROGRESS=$(echo "$INFO" | grep -oP '"verificationprogress"\s*:\s*\K[0-9.]+')
BESTHASH=$(echo "$INFO" | grep -oP '"bestblockhash"\s*:\s*"\K[0-9a-f]+')
[ -n "$HEIGHT" ]   || fail "не удалось получить blocks"
awk "BEGIN{exit !($PROGRESS > 0.9999)}" || fail "нода не синхронизирована (progress=$PROGRESS)"

log "tip height=$HEIGHT hash=$BESTHASH"

# 2. Останавливаем ноду, чтобы chainstate был в консистентном состоянии.
log "останавливаю ноду…"
"$CLI" -datadir="$DATA_DIR" stop || true
for _ in $(seq 1 60); do
    pgrep -x dpowcoind >/dev/null || break
    sleep 2
done
pgrep -x dpowcoind >/dev/null && fail "dpowcoind не остановился"

# 3. Архивируем.
mkdir -p "$WORK_DIR"
TS="$(date -u +%Y%m%d)"
ARCHIVE="$WORK_DIR/dpowcoin-snapshot-${TS}-h${HEIGHT}.tar.zst"
MANIFEST="${ARCHIVE}.sha256"

log "архивирую chainstate + blocks → $ARCHIVE"
tar --use-compress-program="zstd -19 -T0" \
    -cf "$ARCHIVE" \
    -C "$DATA_DIR" \
    chainstate blocks indexes

sha256sum "$ARCHIVE" > "$MANIFEST"
log "sha256: $(cat "$MANIFEST")"

# Опциональная подпись.
if [ -n "$GPG_KEY" ]; then
    gpg --batch --yes --local-user "$GPG_KEY" --armor --detach-sign "$ARCHIVE"
    log "подписан ключом $GPG_KEY"
fi

# 4. Поднять ноду обратно (необязательно — оставить maintainer-у решать).
if [ "${RESTART_AFTER:-1}" = "1" ]; then
    log "запускаю dpowcoind обратно…"
    dpowcoind -datadir="$DATA_DIR" -daemon
fi

# 5. Загружаем на remote.
log "uploading → $REMOTE/"
rclone copy --progress "$ARCHIVE"  "$REMOTE/"
rclone copy --progress "$MANIFEST" "$REMOTE/"
[ -f "${ARCHIVE}.asc" ] && rclone copy "${ARCHIVE}.asc" "$REMOTE/"

# 6. Обновить latest-симлинк (через sidecar JSON).
LATEST_JSON="$WORK_DIR/latest.json"
cat > "$LATEST_JSON" <<EOF
{
  "height": $HEIGHT,
  "bestblockhash": "$BESTHASH",
  "archive": "$(basename "$ARCHIVE")",
  "sha256": "$(awk '{print $1}' "$MANIFEST")",
  "size_bytes": $(stat -c%s "$ARCHIVE"),
  "created": "$(date -Iseconds -u)",
  "url": "$PUBLIC_URL/$(basename "$ARCHIVE")"
}
EOF
rclone copy "$LATEST_JSON" "$REMOTE/"

log "готово: $PUBLIC_URL/$(basename "$ARCHIVE")"

# 7. Локальная ротация.
ls -1t "$WORK_DIR"/dpowcoin-snapshot-*.tar.zst 2>/dev/null | tail -n +"$((KEEP_LOCAL + 1))" | xargs -r rm -f
ls -1t "$WORK_DIR"/dpowcoin-snapshot-*.sha256  2>/dev/null | tail -n +"$((KEEP_LOCAL + 1))" | xargs -r rm -f
