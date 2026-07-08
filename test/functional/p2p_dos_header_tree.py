#!/usr/bin/env python3
# Copyright (c) 2019-2022 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that we reject low difficulty headers to prevent our block tree from filling up with useless bloat.

GENERATOR MODE
--------------
Run with --mine to mine the test blocks and write data/testnet3_headers.json:

    sudo ./build/test/functional/test_runner.py p2p_dos_header_tree --mine

The generator starts the testnet3 node, mines CHECKPOINT_HEIGHT main-chain
blocks and 2 fork blocks (branching from genesis) using Argon2id PoW, then
writes the resulting JSON to data/testnet3_headers.json automatically.

After mining:
  1. Update CHECKPOINT_HEIGHT below if you changed it
  2. Add the checkpoint to chainparams.cpp CTestNetParams::checkpointData:
     {CHECKPOINT_HEIGHT, uint256{"<main[-1].hash from JSON>"}},
  3. Rebuild and run the test without --mine
"""

import json
import os
import time as _time

from test_framework.messages import (
    CBlock,
    CBlockHeader,
    SEQUENCE_FINAL,
    uint256_from_compact,
    calc_pow_hashes,
)
from test_framework.p2p import (
    P2PInterface,
    msg_headers,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.blocktools import (
    NORMAL_GBT_REQUEST_PARAMS,
    create_coinbase,
)
from test_framework.util import assert_equal

# ---------------------------------------------------------------------------
# Only this constant needs updating if you want more/fewer blocks.
# Hashes are read directly from the JSON - no manual copying needed.
# ---------------------------------------------------------------------------
CHECKPOINT_HEIGHT = 580
# ---------------------------------------------------------------------------


def _progress(msg):
    """Write directly to /dev/tty, bypassing all test framework output capture."""
    try:
        with open('/dev/tty', 'w', encoding='utf-8') as tty:
            tty.write(msg + '\n')
            tty.flush()
    except OSError:
        os.write(2, (msg + '\n').encode())


def _header_from_record(rec):
    """Reconstruct a CBlockHeader from a JSON record."""
    hdr = CBlockHeader()
    hdr.nVersion       = rec['version']
    hdr.hashPrevBlock  = int(rec['prev_hash'], 16)
    hdr.hashMerkleRoot = int(rec['merkle_root'], 16)
    hdr.nTime          = rec['time']
    hdr.nBits          = rec['bits']
    hdr.nNonce         = rec['nonce']
    return hdr


# ---------------------------------------------------------------------------
# Generator
# ---------------------------------------------------------------------------

def _run_generator(node, log, datafile_path):
    """Mine CHECKPOINT_HEIGHT main-chain headers + 2 fork headers, write JSON."""

    assert_equal(node.getblockcount(), 0)
    genesis_hash = node.getbestblockhash()
    genesis_hash_int = int(genesis_hash, 16)

    COINBASE_SCRIPT_PUBKEY = bytes.fromhex(
        "76a914eadbac7f36c37e39361168b7aaee3cb24a25312d88ac"
    )

    records_main = []
    records_fork = []  # initialised early so incremental writes work from block 1

    _progress("")
    _progress("=" * 64)
    _progress(f"  GENERATOR MODE - mining {CHECKPOINT_HEIGHT} testnet3 blocks")
    _progress(f"  genesis = {genesis_hash}")
    _progress("=" * 64)

    t_start = _time.monotonic()

    for height in range(1, CHECKPOINT_HEIGHT + 1):
        t_block = _time.monotonic()

        tmpl = node.getblocktemplate(NORMAL_GBT_REQUEST_PARAMS)

        block = CBlock()
        block.nVersion      = 0x20000000
        block.hashPrevBlock = int(tmpl['previousblockhash'], 16)
        block.nTime         = tmpl['curtime']
        block.nBits         = int(tmpl['bits'], 16)

        # halving_period omitted: regtest period (150 blocks) is fine for
        # checkpoint logic testing; we can't physically mine to halving anyway.
        cb = create_coinbase(
            height=height,
            script_pubkey=COINBASE_SCRIPT_PUBKEY,
        )
        cb.nLockTime = 0
        cb.vin[0].nSequence = SEQUENCE_FINAL
        block.vtx = [cb]
        block.hashMerkleRoot = block.calc_merkle_root()

        target = uint256_from_compact(block.nBits)
        found = False
        for nonce in range(0x1_0000_0000):
            block.nNonce   = nonce
            block.argon2id = None
            if calc_pow_hashes(block) <= target:
                found = True
                break
        assert found, f"Exhausted nonce space at height {height}"

        # Populate block.hash (hex string) for submitblock and JSON record.
        block.calc_sha256()

        block_hex = block.serialize(with_witness=False).hex()
        result = node.submitblock(block_hex)
        assert result is None, f"submitblock failed at height {height}: {result}"

        elapsed_block = _time.monotonic() - t_block
        elapsed_total = _time.monotonic() - t_start
        avg_per_block = elapsed_total / height
        remaining     = avg_per_block * (CHECKPOINT_HEIGHT - height)
        eta_min, eta_sec = divmod(int(remaining), 60)

        _progress(
            f"  [{height:>3}/{CHECKPOINT_HEIGHT}]  "
            f"nonce={nonce:<8}  bits={block.nBits:#010x}  "
            f"hash={block.hash[:16]}...  "
            f"{elapsed_block:4.1f}s/block  ETA {eta_min}m{eta_sec:02d}s"
        )

        records_main.append({
            'height':      height,
            'version':     block.nVersion,
            'prev_hash':   f'{block.hashPrevBlock:064x}',
            'merkle_root': f'{block.hashMerkleRoot:064x}',
            'time':        block.nTime,
            'bits':        block.nBits,
            'nonce':       block.nNonce,
            'hash':        block.hash,
        })

        # Write after every block so progress survives a crash.
        with open(datafile_path, 'w', encoding='utf-8') as f:
            json.dump({'main': records_main, 'fork': records_fork}, f, indent=2)
            f.write('\n')

    # Mine 2 fork blocks branching from genesis.
    genesis_block_info = node.getblock(genesis_hash)
    fork_nbits = int(genesis_block_info['bits'], 16)
    fork_target = uint256_from_compact(fork_nbits)

    _progress("")
    _progress(f"  Mining 2 fork headers at genesis nBits={fork_nbits:#010x}")

    records_fork = []
    prev_fork_int = genesis_hash_int

    for height in range(1, 3):
        t_block = _time.monotonic()

        base_time = records_main[height - 1]['time']
        fork_time = base_time + 3600  # +1h offset guarantees distinct hash

        block = CBlock()
        block.nVersion      = 0x20000000
        block.hashPrevBlock = prev_fork_int
        block.nTime         = fork_time
        block.nBits         = fork_nbits

        cb = create_coinbase(
            height=height,
            script_pubkey=COINBASE_SCRIPT_PUBKEY,
        )
        cb.nLockTime = 0
        cb.vin[0].nSequence = SEQUENCE_FINAL
        block.vtx = [cb]
        block.hashMerkleRoot = block.calc_merkle_root()

        found = False
        for nonce in range(0x1_0000_0000):
            block.nNonce   = nonce
            block.argon2id = None
            if calc_pow_hashes(block) <= fork_target:
                found = True
                break
        assert found, f"Exhausted nonce space for fork block at height {height}"

        block.calc_sha256()

        elapsed_block = _time.monotonic() - t_block
        _progress(
            f"  fork [{height}/2]  nonce={nonce:<8}  "
            f"hash={block.hash[:16]}...  {elapsed_block:.1f}s"
        )

        records_fork.append({
            'height':      height,
            'version':     block.nVersion,
            'prev_hash':   f'{block.hashPrevBlock:064x}',
            'merkle_root': f'{block.hashMerkleRoot:064x}',
            'time':        block.nTime,
            'bits':        block.nBits,
            'nonce':       block.nNonce,
            'hash':        block.hash,
        })
        prev_fork_int = int(block.hash, 16)

        with open(datafile_path, 'w', encoding='utf-8') as f:
            json.dump({'main': records_main, 'fork': records_fork}, f, indent=2)
            f.write('\n')

    total_min, total_sec = divmod(int(_time.monotonic() - t_start), 60)
    checkpoint_hash = records_main[-1]['hash']

    _progress("")
    _progress("=" * 64)
    _progress(f"  DONE in {total_min}m{total_sec:02d}s")
    _progress("")
    _progress(f"  JSON written to: {datafile_path}")
    _progress("")
    _progress("  Add to chainparams.cpp CTestNetParams::checkpointData:")
    _progress(f"     {{{CHECKPOINT_HEIGHT}, uint256S(\"{checkpoint_hash}\")}},")
    _progress("")
    _progress("  Then rebuild and run without --mine.")
    _progress("=" * 64)


# ---------------------------------------------------------------------------
# Test
# ---------------------------------------------------------------------------

class RejectLowDifficultyHeadersTest(BitcoinTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.chain = 'testnet3'
        self.num_nodes = 2
        self.extra_args = [["-minimumchainwork=0x0", '-prune=550']] * self.num_nodes

    def add_options(self, parser):
        parser.add_argument(
            '--mine',
            action='store_true',
            default=False,
            help='Generator mode: mine test blocks and write data/testnet3_headers.json, then exit',
        )

    def run_test(self):
        datafile_path = os.path.join(
            os.path.dirname(os.path.realpath(__file__)),
            'data', 'testnet3_headers.json',
        )

        # ----------------------------------------------------------------
        # Generator mode
        # ----------------------------------------------------------------
        if self.options.mine:
            _run_generator(self.nodes[0], self.log, datafile_path)
            return

        # ----------------------------------------------------------------
        # Normal test mode
        # ----------------------------------------------------------------
        self.log.info("Load header data from JSON")
        with open(datafile_path, encoding='utf-8') as f:
            data = json.load(f)

        headers      = [_header_from_record(r) for r in data['main']]
        headers_fork = [_header_from_record(r) for r in data['fork']]

        # Hashes come from the JSON - no hardcoded constants to keep in sync.
        checkpoint_hash = data['main'][-1]['hash']
        fork_tip_hash   = data['fork'][-1]['hash']

        assert len(headers) == CHECKPOINT_HEIGHT, (
            f"JSON has {len(headers)} main blocks but CHECKPOINT_HEIGHT={CHECKPOINT_HEIGHT}. "
            f"Re-run with --mine or update CHECKPOINT_HEIGHT."
        )

        self.log.info("Feed all non-fork headers, including and up to the first checkpoint")
        peer_checkpoint = self.nodes[0].add_p2p_connection(P2PInterface())
        chunk_size = 2000
        for i in range(0, len(headers), chunk_size):
            chunk = headers[i:i+chunk_size]
            peer_checkpoint.send_and_ping(msg_headers(chunk))
        assert {
            'height':    CHECKPOINT_HEIGHT,
            'hash':      checkpoint_hash,
            'branchlen': CHECKPOINT_HEIGHT,
            'status':    'headers-only',
        } in self.nodes[0].getchaintips()

        self.log.info("Feed all fork headers (fails due to checkpoint)")
        with self.nodes[0].assert_debug_log(['bad-fork-prior-to-checkpoint']):
            peer_checkpoint.send_message(msg_headers(headers_fork))
            peer_checkpoint.wait_for_disconnect()

        self.log.info("Feed all fork headers (succeeds without checkpoint)")
        self.restart_node(0, extra_args=['-nocheckpoints', "-minimumchainwork=0x0", '-prune=550'])
        peer_no_checkpoint = self.nodes[0].add_p2p_connection(P2PInterface())
        peer_no_checkpoint.send_and_ping(msg_headers(headers_fork))
        assert {
            'height':    len(headers_fork),
            'hash':      fork_tip_hash,
            'branchlen': len(headers_fork),
            'status':    'headers-only',
        } in self.nodes[0].getchaintips()

        self.log.info("Feed fork headers to node[1] (no checkpoint reached yet - should accept)")
        peer_before_checkpoint = self.nodes[1].add_p2p_connection(P2PInterface())
        peer_before_checkpoint.send_and_ping(msg_headers(headers_fork))
        assert {
            'height':    len(headers_fork),
            'hash':      fork_tip_hash,
            'branchlen': len(headers_fork),
            'status':    'headers-only',
        } in self.nodes[1].getchaintips()


if __name__ == '__main__':
    RejectLowDifficultyHeadersTest().main()
