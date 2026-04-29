#!/usr/bin/env bash
# dpowcoin-loadsnapshot.sh — скачать и развернуть свежий snapshot
# в датадиректорию пользователя, чтобы пропустить долгий IBD.
#
# См. .lumen/docs/14-node-speedup-tutorial.md и §A1 в roadmap.
#
# Usage:
#   ./dpowcoin-loadsnapshot.sh                           # дефолт: ~/.dpowcoin
#   DATA_DIR=/var/lib/dpowcoin ./dpowcoin-loadsnapshot.sh
#   SOURCE=https://snapshot.dpowcore.org ./dpowcoin-loadsnapshot.sh

set -euo pipefail

DATA_DIR="${DATA_DIR:-$HOME/.dpowcoin}"
CLI="${CLI:-dpowcoin-cli}"
DAEMON="${DAEMON:-dpowcoind}"
SOURCE="${SOURCE:-https://snapshot.dpowcore.org}"
TMP_DIR="${TMP_DIR:-/tmp/dpowcoin-loadsnapshot}"
VERIFY_GPG="${VERIFY_GPG:-1}"
GPG_KEY_URL="${GPG_KEY_URL:-https://dpowcore.org/maintainer.asc}"

log()  { printf '[%s] %s\n' "$(date -Iseconds)" "$*"; }
fail() { log "ERROR: $*"; exit 1; }

command -v curl       >/dev/null || fail "curl не установлен"
command -v zstd       >/dev/null || fail "zstd не установлен (apt install zstd)"
command -v sha256sum  >/dev/null || fail "sha256sum нужен"

mkdir -p "$TMP_DIR"

# 1. Получаем latest.json.
log "fetching $SOURCE/latest.json"
curl -fsSL "$SOURCE/latest.json" -o "$TMP_DIR/latest.json"
ARCHIVE_NAME=$(grep -oP '"archive"\s*:\s*"\K[^"]+' "$TMP_DIR/latest.json")
EXPECTED_SHA=$(grep -oP '"sha256"\s*:\s*"\K[0-9a-f]+'    "$TMP_DIR/latest.json")
HEIGHT=$(grep -oP '"height"\s*:\s*\K[0-9]+' "$TMP_DIR/latest.json")
[ -n "$ARCHIVE_NAME" ] || fail "latest.json не парсится"
log "snapshot: $ARCHIVE_NAME (height=$HEIGHT)"

# 2. Скачиваем архив + sha256 (+ подпись).
ARCHIVE_PATH="$TMP_DIR/$ARCHIVE_NAME"
[ -f "$ARCHIVE_PATH" ] || curl -fL --progress-bar "$SOURCE/$ARCHIVE_NAME"           -o "$ARCHIVE_PATH"
curl -fsSL "$SOURCE/${ARCHIVE_NAME}.sha256" -o "${ARCHIVE_PATH}.sha256"
curl -fsSL "$SOURCE/${ARCHIVE_NAME}.asc"    -o "${ARCHIVE_PATH}.asc" 2>/dev/null || true

# 3. Проверка sha256.
ACTUAL_SHA=$(sha256sum "$ARCHIVE_PATH" | awk '{print $1}')
[ "$ACTUAL_SHA" = "$EXPECTED_SHA" ] || fail "sha256 mismatch: $ACTUAL_SHA != $EXPECTED_SHA"
log "sha256 OK"

# 4. (опц.) Проверка подписи.
if [ "$VERIFY_GPG" = "1" ] && [ -f "${ARCHIVE_PATH}.asc" ]; then
    command -v gpg >/dev/null || fail "gpg не установлен (apt install gnupg)"
    if ! gpg --list-keys >/dev/null 2>&1; then
        curl -fsSL "$GPG_KEY_URL" | gpg --import
    fi
    gpg --verify "${ARCHIVE_PATH}.asc" "$ARCHIVE_PATH" || fail "GPG подпись невалидна"
    log "GPG signature OK"
else
    log "WARN: GPG проверка пропущена"
fi

# 5. Останавливаем ноду, если запущена.
if "$CLI" -datadir="$DATA_DIR" getblockcount >/dev/null 2>&1; then
    log "останавливаю работающую ноду…"
    "$CLI" -datadir="$DATA_DIR" stop || true
    for _ in $(seq 1 60); do
        pgrep -x "$DAEMON" >/dev/null || break
        sleep 2
    done
fi

# 6. Бэкапим существующие chainstate/blocks (если есть).
if [ -d "$DATA_DIR/chainstate" ]; then
    BACKUP="$DATA_DIR/.pre-snapshot-$(date -u +%Y%m%d-%H%M%S)"
    mkdir -p "$BACKUP"
    log "перемещаю старые chainstate/blocks/indexes → $BACKUP"
    for d in chainstate blocks indexes; do
        [ -d "$DATA_DIR/$d" ] && mv "$DATA_DIR/$d" "$BACKUP/"
    done
fi
mkdir -p "$DATA_DIR"

# 7. Распаковываем.
log "распаковываю в $DATA_DIR (это займёт несколько минут)…"
tar --use-compress-program="zstd -d -T0" -xf "$ARCHIVE_PATH" -C "$DATA_DIR"

log "готово. Запусти ноду: $DAEMON -daemon"
log "после старта:           $CLI getblockchaininfo"
log "ожидаемый height ≈ $HEIGHT"
