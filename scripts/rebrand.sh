#!/bin/sh
# Авто-rebrand bitcoin→dpowcoin. Регистрозависимо. Исключает vendored crypto / chainparamsseeds.h.
set -eu

EXCLUDE='^(src/crypto/|src/secp256k1/|src/leveldb/|src/minisketch/|src/univalue/|src/chainparamsseeds\.h|\.git/|depends/)'
INCLUDE_DIRS="src/qt test/functional doc share/pixmaps contrib/init contrib/devtools contrib/debian share/setup.nsi.in"

[ -z "$(git status --porcelain)" ] || { echo "FATAL: dirty tree"; exit 1; }

tmp=$(mktemp); trap "rm -f $tmp" EXIT

for d in $INCLUDE_DIRS; do
    [ -e "$d" ] || continue
    find "$d" -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.py' -o -name '*.md' \
         -o -name '*.in' -o -name '*.conf' -o -name '*.sh' -o -name '*.json' \
         -o -name '*.ts' -o -name '*.desktop' -o -name '*.service' -o -name '*.xml' \) \
         > "$tmp"

    while IFS= read -r f; do
        echo "$f" | grep -Eq "$EXCLUDE" && continue
        sed -i \
            -e 's/bitcoin-cli/dpowcoin-cli/g' \
            -e 's/bitcoin-tx/dpowcoin-tx/g' \
            -e 's/bitcoin-wallet/dpowcoin-wallet/g' \
            -e 's/bitcoin-util/dpowcoin-util/g' \
            -e 's/bitcoind/dpowcoind/g' \
            -e 's/bitcoin-qt/dpowcoin-qt/g' \
            -e 's/Bitcoin Core/Dpowcoin Core/g' \
            -e 's/BitcoinCore/DpowcoinCore/g' \
            -e 's/Bitcoin /Dpowcoin /g' \
            -e 's/\bBitcoin\b/Dpowcoin/g' \
            -e 's/\bBITCOIN\b/DPOWCOIN/g' \
            -e 's/\bbitcoin\b/dpowcoin/g' \
            -e 's/\bBTC\b/DPC/g' \
            "$f"
    done < "$tmp"
done

# Переименование файлов (git mv сохраняет историю)
git ls-files | grep -E '(^|/)(bitcoin[-_a-z]*)' | grep -Ev "$EXCLUDE" | while IFS= read -r f; do
    new=$(echo "$f" | sed \
        -e 's|bitcoin-cli|dpowcoin-cli|g' \
        -e 's|bitcoind|dpowcoind|g' \
        -e 's|bitcoin-qt|dpowcoin-qt|g' \
        -e 's|bitcoin-tx|dpowcoin-tx|g' \
        -e 's|bitcoin_|dpowcoin_|g' \
        -e 's|/bitcoin\.|/dpowcoin.|g' \
        -e 's|^bitcoin\.|dpowcoin.|')
    [ "$f" = "$new" ] && continue
    mkdir -p "$(dirname "$new")"
    git mv "$f" "$new"
done

echo "=== diff stat ==="; git diff --stat | tail -20
echo "=== leaked 'Bitcoin' ==="
git grep -n 'Bitcoin' -- $INCLUDE_DIRS | grep -vE '(Copyright|copyright|Satoshi|original|derived from|forked)' | head -30 || echo "clean"
