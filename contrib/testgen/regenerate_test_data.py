#!/usr/bin/env python3
"""
Regenerates all test data files (.hex and .json) for the dpowcoin-tx utility tests.

Run from contrib/testgen/ - no manual copying required:

    cd contrib/testgen
    python3 regenerate_test_data.py

The script automatically locates:
    - dpowcoin-tx binary : <repo>/build/bin/dpowcoin-tx
    - test data files  : <repo>/test/functional/data/util/
    - test spec JSON   : <repo>/test/functional/data/util/bitcoin-util-test.json

The binary must be built first:
    cmake --build build -j$(nproc)

Results (.hex / .json) are written directly back into the data/util/ directory,
so there is nothing to copy manually afterwards.
"""

import json
import os
import subprocess
import sys

# ---------------------------------------------------------------------------
# Path resolution - everything is derived from this script's location.
# Layout assumption:
#   <repo>/contrib/testgen/regenerate_test_data.py   <- this file
#   <repo>/build/bin/dpowcoin-tx                       <- binary
#   <repo>/test/functional/data/util/                <- data + test JSON
# ---------------------------------------------------------------------------

_here    = os.path.dirname(os.path.abspath(__file__))
_repo    = os.path.abspath(os.path.join(_here, "..", ".."))

BINARY_DIR = os.path.join(_repo, "build", "bin")
DATA_DIR   = os.path.join(_repo, "test", "functional", "data", "util")
TEST_JSON  = os.path.join(DATA_DIR, "bitcoin-util-test.json")

# ---------------------------------------------------------------------------
# Main logic
# ---------------------------------------------------------------------------

def main():
    # Sanity checks before doing anything.
    if not os.path.isfile(TEST_JSON):
        sys.exit(f"ERROR: test spec not found:\n  {TEST_JSON}")

    binary_path = os.path.join(BINARY_DIR, "dpowcoin-tx")
    if not os.path.isfile(binary_path):
        sys.exit(
            f"ERROR: dpowcoin-tx not found:\n  {binary_path}\n\n"
            f"Build it first:\n  cmake --build build -j$(nproc)"
        )

    print(f"Binary : {binary_path}")
    print(f"Data   : {DATA_DIR}")
    print()

    with open(TEST_JSON, "r", encoding="utf-8") as f:
        tests = json.load(f)

    total     = 0
    generated = 0
    skipped   = 0
    errors    = 0

    for i, test in enumerate(tests):
        # Skip entries that have no output file to generate.
        if "output_cmp" not in test:
            skipped += 1
            continue

        # Skip entries that are expected to fail (they produce no useful output).
        if test.get("return_code", 0) != 0:
            skipped += 1
            continue

        total += 1

        # The "exec" field names the binary (e.g. "./bitcoin-tx" or "./dpowcoin-tx").
        # We always use our resolved binary_path regardless of the name in JSON,
        # so the test spec does not need to be edited when renaming the binary.
        args        = test.get("args", [])
        output_file = test["output_cmp"]
        input_file  = test.get("input") # optional stdin file
        output_path = os.path.join(DATA_DIR, output_file)

        cmd = [binary_path] + args
        preview = " ".join(cmd[:7]) + (" ..." if len(cmd) > 7 else "")
        print(f"[{i:03d}] {output_file}")
        print(f"      {preview}")

        # Read stdin data if this test pipes a file into the binary.
        stdin_data = None
        if input_file:
            input_path = os.path.join(DATA_DIR, input_file)
            if not os.path.isfile(input_path):
                print(f"      ERROR: input file not found: {input_path}")
                errors += 1
                continue
            with open(input_path, "rb") as fh:
                stdin_data = fh.read().strip() # strip trailing newline from hex files

        # Run the command.
        try:
            result = subprocess.run(
                cmd,
                input=stdin_data,
                capture_output=True,
                timeout=30,
            )
        except subprocess.TimeoutExpired:
            print("      ERROR: command timed out")
            errors += 1
            continue
        except Exception as exc:
            print(f"      ERROR: {exc}")
            errors += 1
            continue

        if result.returncode != 0:
            print(f"      ERROR: exit code {result.returncode}")
            stderr = result.stderr.decode(errors="replace").strip()
            if stderr:
                print(f"      stderr: {stderr}")
            errors += 1
            continue

        # Write the output directly into the data/util/ directory.
        output = result.stdout

        with open(output_path, "wb") as fh:
            fh.write(output)

        print(f"      OK  ({len(output)} bytes written)")
        generated += 1

    # Summary
    print()
    print("=" * 60)
    print(f"Generated : {generated}")
    print(f"Skipped   : {skipped}  (error-expected tests or no output_cmp)")
    print(f"Errors    : {errors}")
    print(f"Total runs: {total}")
    print(f"Data dir  : {DATA_DIR}")


if __name__ == "__main__":
    main()
