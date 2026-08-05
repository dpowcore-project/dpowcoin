#!/usr/bin/env python3
"""
Generates src/test/kernel/block_data.h for dpowcoin.

The header contains 206 serialized regtest blocks required by
src/test/kernel/test_kernel.cpp (btck_chainman_regtest_tests).

The test asserts:
  block[204] (height 205)  — has ≥1 non-coinbase tx (creates P2WPKH 1 BTC output)
  block[205] (height 206)  — has exactly 1 non-coinbase tx spending that P2WPKH output
    coin.GetConfirmationHeight() == 205
    output.Amount()          == 100_000_000  (1 BTC)
    script_pubkey.size()     == 22           (P2WPKH)
  all scripts pass ScriptVerificationFlags::ALL

Run from repo root after building:
    cmake --build build -j$(nproc)
    python3 contrib/testgen/gen_kernel_block_data.py
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

# ---------------------------------------------------------------------------
# Paths — resolved relative to this file
# ---------------------------------------------------------------------------
_HERE = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.abspath(os.path.join(_HERE, "..", ".."))

DPOWCOIND    = os.path.join(_REPO, "build", "bin", "dpowcoind")
DPOWCOIN_CLI = os.path.join(_REPO, "build", "bin", "dpowcoin-cli")
OUTPUT     = os.path.join(_REPO, "src", "test", "kernel", "block_data.h")

BLOCK_COUNT    = 206
REGTEST_GENESIS = "3d96e9f00b7c9a8f9104393435b5f3fd597b5cdd95ae67d9251cfc622a575a22"

RPC_USER = "rpcuser"
RPC_PASS = "rpcpass"
RPC_PORT = 18453

# ---------------------------------------------------------------------------

def die(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def cli(datadir, *args):
    """Run dpowcoin-cli and return parsed JSON (or raw string)."""
    cmd = [
        DPOWCOIN_CLI,
        f"-datadir={datadir}",
        f"-rpcport={RPC_PORT}",
        f"-rpcuser={RPC_USER}",
        f"-rpcpassword={RPC_PASS}",
        "-regtest",
    ] + [str(a) for a in args]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    if r.returncode != 0:
        raise RuntimeError(r.stderr.strip())
    out = r.stdout.strip()
    try:
        return json.loads(out)
    except json.JSONDecodeError:
        return out


def wait_ready(datadir, timeout=90):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            cli(datadir, "getblockcount")
            return
        except Exception:
            time.sleep(0.3)
    die("dpowcoind did not become ready within 90 seconds")


def start_node(datadir):
    os.makedirs(datadir, exist_ok=True)
    with open(os.path.join(datadir, "dpowcoin.conf"), "w") as f:
        f.write(
            "regtest=1\n"
            f"rpcuser={RPC_USER}\n"
            f"rpcpassword={RPC_PASS}\n"
            f"rpcport={RPC_PORT}\n"
            "server=1\n"
            "listen=0\n"
            "[regtest]\n"
            "rpcbind=127.0.0.1\n"
        )
    proc = subprocess.Popen(
        [
            DPOWCOIND,
            f"-datadir={datadir}",
            "-regtest", "-server", "-listen=0", "-noconnect",
            f"-rpcport={RPC_PORT}",
            f"-rpcuser={RPC_USER}",
            f"-rpcpassword={RPC_PASS}",
            "-rpcbind=127.0.0.1", "-rpcallowip=127.0.0.1",
            "-daemon=0", "-printtoconsole=0",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    wait_ready(datadir)
    print(f"  dpowcoind started  pid={proc.pid}")
    return proc


def stop_node(proc, datadir):
    try:
        cli(datadir, "stop")
        proc.wait(timeout=15)
    except Exception:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except Exception:
            proc.kill()
    print("  dpowcoind stopped")


def generate_chain(datadir):
    """
    Mine 206 blocks such that the kernel test passes:

      blocks 1-204  — coinbase-only, mined to wallet (funding)
      block  205    — coinbase + tx_A: wallet coinbase → P2WPKH address (1 BTC)
      block  206    — coinbase + tx_B: P2WPKH output from 205 → wallet

    The wallet is created fresh so all keys are available for signing.
    Coinbase maturity = 100, so blocks 1-104 are spendable by block 204.
    """
    # Create a descriptor wallet for signing
    try:
        cli(datadir, "createwallet", "blockdata_wallet")
    except RuntimeError as e:
        if "already exists" not in str(e):
            die(f"createwallet failed: {e}")

    # Get a bech32 (P2WPKH) address for mining — gives us spendable coinbase
    mine_addr = cli(datadir, "getnewaddress", "", "bech32")
    print(f"  mine address : {mine_addr}")

    # Mine 204 blocks to wallet address.
    # After block 204, coinbase from blocks 1–104 are mature (≥100 confirmations).
    print("  mining 204 coinbase-only blocks to wallet...")
    cli(datadir, "generatetoaddress", 204, mine_addr)
    count = int(cli(datadir, "getblockcount"))
    if count != 204:
        die(f"expected 204 blocks, got {count}")

    # ── block 205 ──────────────────────────────────────────────────────────
    # Send exactly 1.0 BTC to a fresh P2WPKH address.
    # The wallet spends a mature coinbase UTXO and creates a P2WPKH output.
    p2wpkh_addr = cli(datadir, "getnewaddress", "", "bech32")
    print(f"  P2WPKH addr  : {p2wpkh_addr}")

    txid_a = cli(datadir, "sendtoaddress", p2wpkh_addr, "1.00000000")
    print(f"  tx_A         : {txid_a[:32]}...")

    cli(datadir, "generatetoaddress", 1, mine_addr)
    if int(cli(datadir, "getblockcount")) != 205:
        die("block 205 not mined")
    print("  block 205 mined  (coinbase + tx_A creating P2WPKH 1 BTC)")

    # ── block 206 ──────────────────────────────────────────────────────────
    # Find the specific 1 BTC P2WPKH UTXO created by tx_A in block 205.
    utxos = cli(datadir, "listunspent", 1, 1, json.dumps([p2wpkh_addr]))
    p2wpkh_utxo = None
    for u in utxos:
        if u["txid"] == txid_a and round(u["amount"], 8) == 1.0:
            p2wpkh_utxo = u
            break
    if p2wpkh_utxo is None:
        # Relax confirmation filter in case of off-by-one
        utxos = cli(datadir, "listunspent", 0, 9999, json.dumps([p2wpkh_addr]))
        for u in utxos:
            if u["txid"] == txid_a and round(u["amount"], 8) == 1.0:
                p2wpkh_utxo = u
                break
    if p2wpkh_utxo is None:
        die(f"P2WPKH UTXO from tx_A not found. listunspent: {utxos}")

    print(f"  P2WPKH UTXO : txid={p2wpkh_utxo['txid'][:32]}... vout={p2wpkh_utxo['vout']}")

    # Spend it with an explicit raw transaction so the input is exactly the
    # 1 BTC P2WPKH output from block 205 — nothing else.
    # fee ≈ 0.0001 BTC; output must be > 1 sat (test assertion).
    dest_addr = cli(datadir, "getnewaddress", "", "bech32")
    inputs_json  = json.dumps([{"txid": p2wpkh_utxo["txid"], "vout": p2wpkh_utxo["vout"]}])
    outputs_json = json.dumps({dest_addr: "0.99990000"})
    raw_tx = cli(datadir, "createrawtransaction", inputs_json, outputs_json)
    signed = cli(datadir, "signrawtransactionwithwallet", raw_tx)
    if not signed.get("complete"):
        die(f"signing failed: {signed}")

    txid_b = cli(datadir, "sendrawtransaction", signed["hex"])
    print(f"  tx_B         : {txid_b[:32]}...")

    cli(datadir, "generatetoaddress", 1, mine_addr)
    if int(cli(datadir, "getblockcount")) != 206:
        die("block 206 not mined")
    print("  block 206 mined  (coinbase + tx_B spending P2WPKH from height 205)")
    print("  chain complete : 206 blocks")


def export_blocks(datadir):
    """Return list of BLOCK_COUNT raw-hex block strings (heights 1..206)."""
    blocks = []
    for height in range(1, BLOCK_COUNT + 1):
        bh  = cli(datadir, "getblockhash", height)
        raw = cli(datadir, "getblock", bh, "0")
        blocks.append(raw.strip('"'))
        if height % 50 == 0:
            print(f"  exported {height}/{BLOCK_COUNT}")
    print(f"  exported {len(blocks)} blocks total")
    return blocks


def verify_genesis_link(blocks):
    raw      = bytes.fromhex(blocks[0])
    prevhash = raw[4:36][::-1].hex()
    if prevhash != REGTEST_GENESIS:
        die(
            f"block[0] prevhash mismatch:\n"
            f"  got      {prevhash}\n"
            f"  expected {REGTEST_GENESIS}\n"
            f"Wrong binary or genesis?"
        )
    print(f"  genesis link OK  ({prevhash[:16]}...)")


def verify_structure(blocks):
    """Quick sanity-check that blocks 205 and 206 have ≥2 transactions."""
    for label, idx in [("block 205", 204), ("block 206", 205)]:
        raw = bytes.fromhex(blocks[idx])
        # Skip 80-byte header, then read varint tx count
        tx_count_byte = raw[80]
        if tx_count_byte < 2:
            die(f"{label} (index {idx}) has only {tx_count_byte} tx — expected ≥2")
        print(f"  {label}: tx_count={tx_count_byte}  ✓")


def write_header(blocks, path):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w") as f:
        f.write(
            "// Copyright (c) The Bitcoin Core developers\n"
            "// Distributed under the MIT software license, see the accompanying\n"
            "// file COPYING or https://opensource.org/license/mit.\n"
            "// Generated by contrib/testgen/gen_kernel_block_data.py\n"
            "\n"
            "#ifndef BITCOIN_TEST_KERNEL_BLOCK_DATA_H\n"
            "#define BITCOIN_TEST_KERNEL_BLOCK_DATA_H\n"
            "#include <array>\n"
            "#include <string_view>\n"
            f"inline constexpr std::array<std::string_view, {len(blocks)}> REGTEST_BLOCK_DATA {{\n"
        )
        for i, raw in enumerate(blocks):
            comma = "," if i < len(blocks) - 1 else ""
            f.write(f'"{raw}"{comma}\n')
        f.write("};\n#endif // BITCOIN_TEST_KERNEL_BLOCK_DATA_H\n")
    print(f"  written: {path}")


def main():
    for name, path in [("dpowcoind", DPOWCOIND), ("dpowcoin-cli", DPOWCOIN_CLI)]:
        if not os.path.isfile(path):
            die(f"{name} not found: {path}\nBuild first:\n  cmake --build build -j$(nproc)")

    print(f"dpowcoind  : {DPOWCOIND}")
    print(f"output   : {OUTPUT}")
    print()

    datadir = tempfile.mkdtemp(prefix="dpowcoin_blockdata_")
    proc    = None
    blocks  = None
    try:
        print("[1/4] Starting dpowcoind...")
        proc = start_node(datadir)

        print("\n[2/4] Mining chain...")
        generate_chain(datadir)

        print("\n[3/4] Exporting blocks...")
        blocks = export_blocks(datadir)
        verify_genesis_link(blocks)
        verify_structure(blocks)

    finally:
        print("\n[4/4] Stopping dpowcoind...")
        if proc:
            stop_node(proc, datadir)
        shutil.rmtree(datadir, ignore_errors=True)

    if blocks is None:
        die("block export failed")

    print("\nWriting header...")
    write_header(blocks, OUTPUT)

    print(f"\nDone. {len(blocks)} blocks -> {OUTPUT}")
    print(
        "\nNext steps:\n"
        "  git add src/test/kernel/block_data.h\n"
        "  git commit -m 'test: regenerate kernel block_data.h for dpowcoin regtest'"
    )


if __name__ == "__main__":
    main()
