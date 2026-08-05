#!/usr/bin/env python3

import json
import sys
import subprocess
from pathlib import Path


def main():
    """Tests ordered roughly from faster to slower."""
    expect_code(run_verify("", "pub", '0.32'), 4, "Nonexistent version should fail")
    expect_code(run_verify("", "pub", '0.32.awefa.12f9h'), 11, "Malformed version should fail")
    expect_code(run_verify('--min-good-sigs 20', "pub", "26.3.2"), 9, "--min-good-sigs 20 should fail")

    print("- testing verification (26.3.2-x86_64-linux-gnu.tar.gz)", flush=True)
    _220_x86_64_linux_gnu = run_verify("--json", "pub", "26.3.2-x86_64-linux-gnu.tar.gz")
    try:
        result = json.loads(_220_x86_64_linux_gnu.stdout.decode())
    except Exception:
        print("failed on 26.3.2-x86_64-linux-gnu.tar.gz --json:")
        print_process_failure(_220_x86_64_linux_gnu)
        raise

    expect_code(_220_x86_64_linux_gnu, 0, "26.3.2-x86_64-linux-gnu.tar.gz should succeed")
    v = result['verified_binaries']
    assert result['good_trusted_sigs']
    assert len(v) == 1
    assert v['dpowcoin-26.3.2-x86_64-linux-gnu.tar.gz'] == '98a000effaef1a4de27e935592413e9a81f6d4f6af95e6ed1dc6337643e5e534'

    print("- testing verification (26.3.2)", flush=True)
    _220 = run_verify("--json", "pub", "26.3.2")
    try:
        result = json.loads(_220.stdout.decode())
    except Exception:
        print("failed on 26.3.2 --json:")
        print_process_failure(_220)
        raise

    expect_code(_220, 0, "26.3.2 should succeed")
    v = result['verified_binaries']
    assert result['good_trusted_sigs']
    assert v['dpowcoin-26.3.2-aarch64-linux-gnu.tar.gz'] == '7f1c73a43545850b45ed32be64bc742b81b143d6719aea03f8038ed1941475a3'
    assert v['dpowcoin-26.3.2-x86_64-apple-darwin.tar.gz'] == '5432f1a21be17b98421cef7a2e6471f80e4f9cd5032572d42612466442378526'
    assert v['dpowcoin-26.3.2-x86_64-linux-gnu.tar.gz'] == '98a000effaef1a4de27e935592413e9a81f6d4f6af95e6ed1dc6337643e5e534'


def run_verify(global_args: str, command: str, command_args: str) -> subprocess.CompletedProcess:
    maybe_here = Path.cwd() / 'verify.py'
    path = maybe_here if maybe_here.exists() else Path.cwd() / 'contrib' / 'verify-binaries' / 'verify.py'

    if command == "pub":
        command += " --cleanup"

    return subprocess.run(
        f"{path} {global_args} {command} {command_args}",
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)


def expect_code(completed: subprocess.CompletedProcess, expected_code: int, msg: str):
    if completed.returncode != expected_code:
        print(f"{msg!r} failed: got code {completed.returncode}, expected {expected_code}")
        print_process_failure(completed)
        sys.exit(1)
    else:
        print(f"✓ {msg!r} passed")


def print_process_failure(completed: subprocess.CompletedProcess):
    print(f"stdout:\n{completed.stdout.decode()}")
    print(f"stderr:\n{completed.stderr.decode()}")


if __name__ == '__main__':
    main()
