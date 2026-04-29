#!/usr/bin/env bash
# contrib/devtools/bench_argon2_cache.sh
#
# Quick A/B benchmark: time `dpowcoind -reindex` (or -reindex-chainstate)
# with the LRU PoW cache ENABLED vs DISABLED. The cache is controlled by
# the `-powcachesize=N` startup option (N=0 disables both caches).
#
# Usage:
#   ./bench_argon2_cache.sh [<datadir>] [<height_target>]
#
# Defaults:
#   datadir       = $HOME/.dpowcoin
#   height_target = current tip (whatever -reindex finishes with)
#
# Requirements:
#   - Built dpowcoind in dpowcoin-src/build/bin/
#   - Existing block files (blocks/blk*.dat); chainstate is wiped each run.
#   - At least ~20 GB free disk for the duplicate datadir.
#
# Method (per run):
#   1) cp -al datadir to a tmp shadow dir (hardlinks; cheap)
#   2) rm -rf shadow/chainstate  (forces full UTXO rebuild via -reindex-chainstate)
#   3) /usr/bin/time -v dpowcoind -daemon=0 -reindex-chainstate -powcachesize=N -datadir=<shadow>
#   4) tail dpowcoind log for "progress=1.000000" or matched height
#   5) record wall time, peak RSS, cache hits/misses (from getindexinfo / debug.log)

set -euo pipefail

DATADIR="${1:-$HOME/.dpowcoin}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DPOWCOIND="$ROOT/dpowcoin-src/build/bin/dpowcoind"
DPOWCOIN_CLI="$ROOT/dpowcoin-src/build/bin/dpowcoin-cli"
RESULTS_DIR="$ROOT/.lumen/bench-results"
mkdir -p "$RESULTS_DIR"

if [[ ! -x "$DPOWCOIND" ]]; then
    echo "ERROR: $DPOWCOIND not found. Build first with cmake --build dpowcoin-src/build -j\$(nproc)" >&2
    exit 1
fi

run_one() {
    local label="$1"
    local capacity="$2"
    local shadow="/tmp/dpowcoin-bench-$label-$$"
    local logfile="$RESULTS_DIR/${label}-$(date +%Y%m%d-%H%M%S).log"

    echo "==> [$label] capacity=$capacity  shadow=$shadow"
    cp -al "$DATADIR" "$shadow"
    rm -rf "$shadow/chainstate"

    /usr/bin/time -v -o "$logfile.time" \
        "$DPOWCOIND" \
            -daemon=0 \
            -datadir="$shadow" \
            -reindex-chainstate \
            -powcachesize="$capacity" \
            -printtoconsole=0 \
            > "$logfile" 2>&1 &
    local pid=$!

    # Wait for tip ("progress=1.000000" appears in debug.log when caught up).
    while kill -0 "$pid" 2>/dev/null; do
        if grep -q "progress=1.000000" "$shadow/debug.log" 2>/dev/null; then
            "$DPOWCOIN_CLI" -datadir="$shadow" stop >/dev/null 2>&1 || true
            break
        fi
        sleep 5
    done
    wait "$pid" || true

    echo "==> [$label] done. Stats:"
    grep -E "wall clock|Maximum resident" "$logfile.time" || true
    echo "----"
    rm -rf "$shadow"
}

echo "Benchmark: PoW LRU cache A/B"
echo "Source datadir: $DATADIR"
echo "Results: $RESULTS_DIR"
echo

run_one "off"   0
run_one "on"    100000

echo "Done. Compare *.time files in $RESULTS_DIR"
