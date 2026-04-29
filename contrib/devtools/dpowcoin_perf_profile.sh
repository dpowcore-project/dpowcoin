#!/usr/bin/env bash
# Copyright (c) 2026 The Dpowcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# dpowcoin_perf_profile.sh — wrapper around `perf record` + flamegraph for
# the Dual PoW microbench targets.
#
# Usage:
#   contrib/devtools/dpowcoin_perf_profile.sh [filter] [out-dir]
#
# Examples:
#   contrib/devtools/dpowcoin_perf_profile.sh DpowArgon2idRaw build/perf
#   contrib/devtools/dpowcoin_perf_profile.sh 'Dpow.*' build/perf
#
# Requirements:
#   * linux-tools-common / linux-tools-$(uname -r) (for `perf`)
#   * Optional: FlameGraph repo on $PATH (stackcollapse-perf.pl, flamegraph.pl)
#
# This script never modifies sources; it only profiles an already-built
# bench_bitcoin binary. Run a build first:
#   cmake --build build --target bench_bitcoin -j$(nproc)

set -euo pipefail

FILTER="${1:-Dpow.*}"
OUT_DIR="${2:-build/perf}"
BENCH_BIN="${BENCH_BIN:-build/bin/bench_bitcoin}"

if [[ ! -x "${BENCH_BIN}" ]]; then
  echo "[ERR] bench_bitcoin not found at ${BENCH_BIN}" >&2
  echo "      Build first: cmake --build build --target bench_bitcoin -j\$(nproc)" >&2
  exit 1
fi

if ! command -v perf >/dev/null 2>&1; then
  echo "[ERR] 'perf' not found. Install linux-tools-$(uname -r)." >&2
  exit 1
fi

mkdir -p "${OUT_DIR}"
PERF_DATA="${OUT_DIR}/perf-$(date +%Y%m%d-%H%M%S).data"
PERF_TXT="${PERF_DATA%.data}.report.txt"
FOLDED="${PERF_DATA%.data}.folded"
SVG="${PERF_DATA%.data}.svg"

echo "[*] Recording: ${BENCH_BIN} -filter='${FILTER}' -min-time=500"
echo "[*] perf data → ${PERF_DATA}"

# -F 999  ~1kHz sampling
# --call-graph dwarf  needs frame info but works without -fno-omit-frame-pointer
perf record -F 999 --call-graph dwarf -o "${PERF_DATA}" -- \
  "${BENCH_BIN}" -filter="${FILTER}" -min-time=500 >/dev/null

echo "[*] perf report → ${PERF_TXT}"
perf report -i "${PERF_DATA}" --stdio --no-children 2>/dev/null \
  | head -200 > "${PERF_TXT}" || true

if command -v stackcollapse-perf.pl >/dev/null 2>&1 \
   && command -v flamegraph.pl >/dev/null 2>&1; then
  echo "[*] Folding stacks → ${FOLDED}"
  perf script -i "${PERF_DATA}" 2>/dev/null \
    | stackcollapse-perf.pl > "${FOLDED}"
  echo "[*] Flamegraph    → ${SVG}"
  flamegraph.pl --title "Dpowcoin Dual PoW — ${FILTER}" "${FOLDED}" > "${SVG}"
else
  echo "[i] FlameGraph scripts not on PATH — skipping SVG."
  echo "    git clone https://github.com/brendangregg/FlameGraph and add to PATH."
fi

echo
echo "[OK] Top hotspots (head):"
head -40 "${PERF_TXT}" || true
echo
echo "Artifacts in ${OUT_DIR}:"
ls -1 "${OUT_DIR}"
