#!/usr/bin/env bash
# Copyright (c) 2026 The Dpowcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Wrapper around test/functional/test_runner.py that injects the
# Dpowcoin skip-list (dpowcoin_skip.txt). Tests listed there are
# fundamentally incompatible with Dual PoW / LWMA — see
# dpowcoin-skip-list.md for the rationale per entry.
#
# Usage:
#   test/functional/dpowcoin_test_runner.sh                 # full suite minus skip-list
#   test/functional/dpowcoin_test_runner.sh -- --extended   # forwards extra args verbatim

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
SKIP_FILE="${SCRIPT_DIR}/dpowcoin_skip.txt"

if [[ ! -f "${SKIP_FILE}" ]]; then
    echo "error: skip-list not found at ${SKIP_FILE}" >&2
    exit 2
fi

# Build --exclude=<test> args from non-comment, non-empty lines.
mapfile -t EXCLUDES < <(grep -Ev '^[[:space:]]*(#|$)' "${SKIP_FILE}")

EXCLUDE_ARGS=()
for t in "${EXCLUDES[@]}"; do
    EXCLUDE_ARGS+=("--exclude=${t}")
done

echo "Dpowcoin: skipping ${#EXCLUDES[@]} test(s) — see dpowcoin-skip-list.md"
exec "${SCRIPT_DIR}/test_runner.py" "${EXCLUDE_ARGS[@]}" "$@"
