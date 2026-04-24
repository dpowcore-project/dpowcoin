#!/bin/sh
# POSIX. Применяет 5 custom-патчей Dual PoW последовательно, с чекпойнт-тегами и rollback.
#
# Usage:
#   VER=30.2 scripts/apply-custom-layer.sh              # все 5
#   VER=30.2 scripts/apply-custom-layer.sh --only 01    # только 01-crypto-libs
#   VER=30.2 scripts/apply-custom-layer.sh --only "01 02"   # 01 и 02
#
# ART=path/to/rebase-artifacts (default: ../.lumen/rebase-artifacts)
set -eu

ART="${ART:-../.lumen/rebase-artifacts}"
VER="${VER:?set VER=30.2}"
PATCHES_ALL="01-crypto-libs 02-dualpow-hashing 03-lwma-daa-and-validation 04-chainparams 05-rpc-dualpow"
PATCHES="$PATCHES_ALL"

# --only filter: "--only 01" | "--only '01 02'"
if [ "${1:-}" = "--only" ]; then
    shift
    [ $# -ge 1 ] || { echo "FATAL: --only requires an argument"; exit 1; }
    wanted="$1"
    filtered=""
    for p in $PATCHES_ALL; do
        prefix=$(echo "$p" | cut -d- -f1)
        for w in $wanted; do
            if [ "$prefix" = "$w" ]; then
                filtered="$filtered $p"
            fi
        done
    done
    [ -n "$filtered" ] || { echo "FATAL: --only '$wanted' matched nothing"; exit 1; }
    PATCHES="$filtered"
fi

# Preflight
[ -z "$(git status --porcelain)" ] || { echo "FATAL: dirty tree"; exit 1; }
git rev-parse --verify "bitcoin-v${VER}" >/dev/null || { echo "FATAL: tag bitcoin-v${VER} missing"; exit 1; }

start_sha=$(git rev-parse HEAD)
echo "Start SHA: $start_sha"
echo "Applying:  $PATCHES"

for p in $PATCHES; do
    f="$ART/${p}.patch"
    [ -f "$f" ] || { echo "FATAL: $f missing"; exit 1; }
    echo "=== Applying $p ==="

    if git apply --check "$f" 2>/dev/null; then
        git apply "$f"
    elif git apply --3way --check "$f" 2>/dev/null; then
        git apply --3way "$f" || {
            echo "CONFLICTS in $p — resolve manually, then git add -A && rerun"
            git tag -f "checkpoint/${VER}/failed-${p}"
            exit 2
        }
    else
        echo "FATAL: $p does not apply even with 3-way."
        git apply --3way "$f" 2>&1 | head -30
        git reset --hard "$start_sha"
        exit 3
    fi

    git add -A
    case "$p" in
      01-*) msg="dpowcoin: crypto: vendor yespower and argon2d libraries" ;;
      02-*) msg="dpowcoin: pow: add Dual PoW hashing (yespower + argon2id)" ;;
      03-*) msg="dpowcoin: consensus: LWMA-1 DAA + dual-hash validation" ;;
      04-*) msg="dpowcoin: chainparams: mainnet/testnet/regtest + seeds" ;;
      05-*) msg="dpowcoin: rpc: expose dual-hash fields" ;;
    esac
    git commit -m "$msg" -m "Applied from ${p}.patch during rebase to bitcoin-v${VER}."
    git tag -f "checkpoint/${VER}/after-${p}"
done

echo "Done. HEAD=$(git rev-parse --short HEAD)"
