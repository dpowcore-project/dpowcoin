#!/usr/bin/env python3
# Copyright (c) 2024 The Dpowcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test getargon2idpowblockhash RPC command."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)


class Argon2idPowBlockHashTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        node = self.nodes[0]

        self.generate(node, 10)

        # --- Basic sanity: returns a 64-char hex string ---
        pow_hash = node.getargon2idpowblockhash(height=1)
        assert_equal(len(pow_hash), 64)
        # must be valid hex
        int(pow_hash, 16)

        # --- Core invariant: argon2id PoW hash != SHA256d block hash ---
        block_hash = node.getblockhash(height=1)
        assert pow_hash != block_hash, (
            "getargon2idpowblockhash should differ from getblockhash "
            "(argon2id vs SHA256d)"
        )

        # --- Named argument works ---
        assert_equal(node.getargon2idpowblockhash(height=1), pow_hash)

        # --- Genesis block (height=0) ---
        pow_hash_0 = node.getargon2idpowblockhash(height=0)
        assert_equal(len(pow_hash_0), 64)

        # --- Out-of-range heights return RPC error -8 ---
        assert_raises_rpc_error(-8, "Block height out of range",
                                node.getargon2idpowblockhash, height=-1)
        assert_raises_rpc_error(-8, "Block height out of range",
                                node.getargon2idpowblockhash, height=9999)

        self.log.info("getargon2idpowblockhash: all checks passed")


if __name__ == '__main__':
    Argon2idPowBlockHashTest(__file__).main()
