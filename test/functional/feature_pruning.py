#!/usr/bin/env python3
# Copyright (c) 2014-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the pruning code.

WARNING:
This test uses 4GB of disk space.
"""
import os

from test_framework.blocktools import (
    MAX_FUTURE_BLOCK_TIME,
    TIMESTAMP_WINDOW,
    MIN_BLOCKS_TO_KEEP,
    create_block,
    create_coinbase,
)
from test_framework.script import (
    CScript,
    OP_NOP,
    OP_RETURN,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
    try_rpc,
)

# Rescans start at the earliest block up to 2 hours before a key timestamp, so
# the manual prune RPC avoids pruning blocks in the same window to be
# compatible with pruning based on key creation time.

# Extra seconds added to mocktime beyond the last block's nTime to ensure all
# mine_large_blocks() submissions are accepted under MAX_FUTURE_BLOCK_TIME=600.
MOCK_BUFFER = MAX_FUTURE_BLOCK_TIME + 100


def set_mocktime(nodes, mock_time):
    for node in nodes:
        node.setmocktime(mock_time)


def set_mocktime_for_large_blocks(nodes, n_blocks):
    best_time = nodes[0].getblock(nodes[0].getbestblockhash())["time"]
    mock_time = best_time + n_blocks + MOCK_BUFFER
    for node in nodes:
        node.setmocktime(mock_time)
    return mock_time


def reset_mocktime(nodes):
    for node in nodes:
        node.setmocktime(0)


def mine_large_blocks(node, n):
    # Make a large scriptPubKey for the coinbase transaction. This is OP_RETURN
    # followed by 950k of OP_NOP. This would be non-standard in a non-coinbase
    # transaction but is consensus valid.

    # Set the nTime if this is the first time this function has been called.
    # A static variable ensures that time is monotonicly increasing and is therefore
    # different for each block created => blockhash is unique.
    if "nTime" not in mine_large_blocks.__dict__:
        mine_large_blocks.nTime = 0

    # Get the block parameters for the first block
    big_script = CScript([OP_RETURN] + [OP_NOP] * 950000)
    best_block = node.getblock(node.getbestblockhash())
    height = int(best_block["height"]) + 1
    mine_large_blocks.nTime = max(mine_large_blocks.nTime, int(best_block["time"])) + 1
    previousblockhash = int(best_block["hash"], 16)

    for _ in range(n):
        block = create_block(hashprev=previousblockhash, ntime=mine_large_blocks.nTime, coinbase=create_coinbase(height, script_pubkey=big_script))
        block.solve()

        # Submit to the node
        node.submitblock(block.serialize().hex())

        previousblockhash = block.hash_int
        height += 1
        mine_large_blocks.nTime += 1

def calc_usage(blockdir):
    return sum(os.path.getsize(blockdir + f) for f in os.listdir(blockdir) if os.path.isfile(os.path.join(blockdir, f))) / (1024. * 1024.)

class PruneTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 6
        self.uses_wallet = None

        # Create nodes 0 and 1 to mine.
        # Create node 2 to test pruning.
        self.full_node_default_args = ["-maxreceivebuffer=20000", "-checkblocks=5"]
        # Create nodes 3 and 4 to test manual pruning (they will be re-started with manual pruning later)
        # Create nodes 5 to test wallet in prune mode, but do not connect
        self.extra_args = [
            self.full_node_default_args,
            self.full_node_default_args,
            ["-maxreceivebuffer=20000", "-prune=550"],
            ["-maxreceivebuffer=20000"],
            ["-maxreceivebuffer=20000"],
            ["-prune=550", "-blockfilterindex=1"],
        ]
        self.rpc_timeout = 1200

    def setup_network(self):
        self.setup_nodes()

        self.prunedir = os.path.join(self.nodes[2].blocks_path, '')

        self.connect_nodes(0, 1)
        self.connect_nodes(1, 2)
        self.connect_nodes(0, 2)
        self.connect_nodes(0, 3)
        self.connect_nodes(0, 4)
        self.sync_blocks(self.nodes[0:5])

    def setup_nodes(self):
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()
        if self.is_wallet_compiled():
            self.import_deterministic_coinbase_privkeys()

    def restart_node_mocktime(self, node_number, extra_args=None):
        """Restart a node, injecting -mocktime so the startup block-timestamp check
        does not reject our own chain whose tip >> real wall-clock time.

        After the node is up we immediately clear mocktime via setmocktime(0) so
        the rest of the test runs on real time (or its own next mocktime call).
        """
        tip_time = self.nodes[node_number].getblock(
            self.nodes[node_number].getbestblockhash()
        )["time"]
        mock_arg = f"-mocktime={tip_time + MOCK_BUFFER}"
        all_args = list(extra_args or []) + [mock_arg]
        self.restart_node(node_number, extra_args=all_args)
        self.nodes[node_number].setmocktime(0)

    def start_node_mocktime(self, node_number, tip_time, extra_args=None):
        """Start a stopped node (whose tip_time is known from another node) with
        -mocktime so startup block validation accepts future-timestamped blocks.
        Clears mocktime immediately after the node is ready.
        """
        mock_arg = f"-mocktime={tip_time + MOCK_BUFFER}"
        all_args = list(extra_args or []) + [mock_arg]
        self.start_node(node_number, extra_args=all_args)
        self.nodes[node_number].setmocktime(0)

    def create_big_chain(self):
        # Start by creating some coinbases we can spend later
        self.generate(self.nodes[1], 200, sync_fun=lambda: self.sync_blocks(self.nodes[0:2]))
        self.generate(self.nodes[0], 150, sync_fun=self.no_op)

        # Then mine enough full blocks to create more than 550MiB of data
        set_mocktime_for_large_blocks(self.nodes[0:5], 645)
        mine_large_blocks(self.nodes[0], 645)
        self.wait_until(
            lambda: all(len(n.getpeerinfo()) > 0 for n in self.nodes[0:5]),
            timeout=120
        )
        self.sync_blocks(self.nodes[0:5])  # FIX: sync before reset so nodes accept future-timestamp blocks
        reset_mocktime(self.nodes[0:5])

    def test_invalid_command_line_options(self):
        self.stop_node(0)
        self.nodes[0].assert_start_raises_init_error(
            expected_msg='Error: Prune cannot be configured with a negative value.',
            extra_args=['-prune=-1'],
        )
        self.nodes[0].assert_start_raises_init_error(
            expected_msg='Error: Prune configured below the minimum of 550 MiB.  Please use a higher number.',
            extra_args=['-prune=549'],
        )
        self.nodes[0].assert_start_raises_init_error(
            expected_msg='Error: Prune mode is incompatible with -txindex.',
            extra_args=['-prune=550', '-txindex'],
        )
        self.nodes[0].assert_start_raises_init_error(
            expected_msg='Error: Prune mode is incompatible with -reindex-chainstate. Use full -reindex instead.',
            extra_args=['-prune=550', '-reindex-chainstate'],
        )

    def test_rescan_blockchain(self):
        # FIX: node 0 tip >> real clock; pass -mocktime at startup then clear it.
        self.restart_node_mocktime(0, extra_args=["-prune=550"])
        assert_raises_rpc_error(-1, "Can't rescan beyond pruned data. Use RPC call getblockchaininfo to determine your pruned height.", self.nodes[0].rescanblockchain)

    def test_height_min(self):
        assert os.path.isfile(os.path.join(self.prunedir, "blk00000.dat")), "blk00000.dat is missing, pruning too early"
        self.log.info("Success")
        self.log.info(f"Though we're already using more than 550MiB, current usage: {calc_usage(self.prunedir)}")
        self.log.info("Mining 25 more blocks should cause the first block file to be pruned")
        # Pruning doesn't run until we're allocating another chunk, 20 full blocks past the height cutoff will ensure this
        set_mocktime_for_large_blocks(self.nodes[0:3], 25)  # nodes 3,4 are stopped at this point
        mine_large_blocks(self.nodes[0], 25)
        self.sync_blocks(self.nodes[0:3])  # FIX: sync before reset so nodes accept future-timestamp blocks
        reset_mocktime(self.nodes[0:3])

        # Wait for blk00000.dat to be pruned
        self.wait_until(lambda: not os.path.isfile(os.path.join(self.prunedir, "blk00000.dat")), timeout=30)

        self.log.info("Success")
        usage = calc_usage(self.prunedir)
        self.log.info(f"Usage should be below target: {usage}")
        assert_greater_than(550, usage)

    def create_chain_with_staleblocks(self):
        # Create stale blocks in manageable sized chunks
        self.log.info("Mine 24 (stale) blocks on Node 1, followed by 25 (main chain) block reorg from Node 0, for 12 rounds")

        for _ in range(12):
            # Disconnect node 0 so it can mine a longer reorg chain without knowing about node 1's soon-to-be-stale chain
            # Node 2 stays connected, so it hears about the stale blocks and then reorg's when node0 reconnects
            self.disconnect_nodes(0, 1)
            self.disconnect_nodes(0, 2)
            # Mine 24 blocks in node 1
            set_mocktime_for_large_blocks([self.nodes[0], self.nodes[1], self.nodes[2]], 24)
            mine_large_blocks(self.nodes[1], 24)

            # Reorg back with 25 block chain from node 0
            set_mocktime_for_large_blocks([self.nodes[0], self.nodes[1], self.nodes[2]], 25)
            mine_large_blocks(self.nodes[0], 25)

            # Create connections in the order so both nodes can see the reorg at the same time
            # mocktime must remain set during sync so nodes accept the reorg blocks (FTL=600)
            self.connect_nodes(0, 1)
            self.connect_nodes(0, 2)
            self.sync_blocks(self.nodes[0:3])
            reset_mocktime([self.nodes[0], self.nodes[1], self.nodes[2]])

        self.log.info(f"Usage can be over target because of high stale rate: {calc_usage(self.prunedir)}")

    def reorg_test(self):
        # Node 1 will mine a 300 block chain starting 287 blocks back from Node 0 and Node 2's tip
        # This will cause Node 2 to do a reorg requiring 288 blocks of undo data to the reorg_test chain

        height = self.nodes[1].getblockcount()
        self.log.info(f"Current block height: {height}")

        self.forkheight = height - 287
        self.forkhash = self.nodes[1].getblockhash(self.forkheight)
        self.log.info(f"Invalidating block {self.forkhash} at height {self.forkheight}")
        self.nodes[1].invalidateblock(self.forkhash)

        # We've now switched to our previously mined-24 block fork on node 1, but that's not what we want
        # So invalidate that fork as well, until we're on the same chain as node 0/2 (but at an ancestor 288 blocks ago)
        mainchainhash = self.nodes[0].getblockhash(self.forkheight - 1)
        curhash = self.nodes[1].getblockhash(self.forkheight - 1)
        while curhash != mainchainhash:
            self.nodes[1].invalidateblock(curhash)
            curhash = self.nodes[1].getblockhash(self.forkheight - 1)

        assert self.nodes[1].getblockcount() == self.forkheight - 1
        self.log.info(f"New best height: {self.nodes[1].getblockcount()}")

        # Disconnect node1 and generate the new chain
        self.disconnect_nodes(0, 1)
        self.disconnect_nodes(1, 2)

        self.log.info("Generating new longer chain of 300 more blocks")
        # node 1 is at forkheight-1; its tip timestamp is already >> real time,
        # so generate() hits FTL=600 without mocktime. Set on all three so they
        # also accept node 1's chain during sync after reconnect.
        best_time = self.nodes[1].getblock(self.nodes[1].getbestblockhash())["time"]
        mock_time = best_time + 300 + MOCK_BUFFER
        set_mocktime(self.nodes[0:3], mock_time)  # FIX: use helper instead of inline loop
        self.generate(self.nodes[1], 300, sync_fun=self.no_op)

        self.log.info("Reconnect nodes")
        self.connect_nodes(0, 1)
        self.connect_nodes(1, 2)
        self.sync_blocks(self.nodes[0:3], timeout=120)
        reset_mocktime(self.nodes[0:3])

        self.log.info(f"Verify height on node 2: {self.nodes[2].getblockcount()}")
        self.log.info(f"Usage possibly still high because of stale blocks in block files: {calc_usage(self.prunedir)}")

        self.log.info("Mine 220 more large blocks so we have requisite history")

        set_mocktime_for_large_blocks(self.nodes[0:3], 220)
        mine_large_blocks(self.nodes[0], 220)
        self.sync_blocks(self.nodes[0:3], timeout=120)
        reset_mocktime(self.nodes[0:3])

        usage = calc_usage(self.prunedir)
        self.log.info(f"Usage should be below target: {usage}")
        assert_greater_than(550, usage)

    def nodes_linked(self, a, b):
        """Check whether nodes[a] and nodes[b] are currently connected to
        each other (mirrors the subversion-matching logic connect_nodes()
        itself uses internally)."""
        subver_a = self.nodes[a].getnetworkinfo()['subversion']
        subver_b = self.nodes[b].getnetworkinfo()['subversion']
        linked_from_b = any(p['subver'] == subver_a for p in self.nodes[b].getpeerinfo())
        linked_from_a = any(p['subver'] == subver_b for p in self.nodes[a].getpeerinfo())
        return linked_from_a and linked_from_b

    def wait_for_height_with_reconnect(self, node_a, node_b, height_getter, goal_height, timeout=1200):
        """wait_until() variant for redownload/reorg waits that can outlive
        a single P2P connection.

        BACKPORT-ADJACENT FIX (bitweb-local, not from upstream bitcoin/bitcoin):
        connect_nodes()/addnode("onetry") establishes a ConnectionType::MANUAL
        link. Manual connections are NOT retried by the node's own
        ThreadOpenAddedConnections()/addrman outbound logic -- that automatic
        reconnect only applies to `addnode add` (persistent) peers. If the
        link stalls mid-transfer (net_processing.cpp BLOCK_STALLING_TIMEOUT
        fires -> "Timeout downloading block ..., disconnecting"), the socket
        is closed permanently and no one ever calls connect_nodes() again.
        Under a fast/idle machine this basically never triggers (233 blocks
        relay in a couple seconds), which is why the test is stable when run
        alone -- but under full test-suite CPU/disk contention the transfer
        can slow down enough to hit the stall timeout, and the node then
        waits forever for blocks that will never arrive.
        This has nothing to do with the m_blocks_unlinked/AddUnlinkedBlock
        C++ backport; it reproduces identically with that backport fully
        reverted, as long as the invalidateblock height stays at 1295 (i.e.
        the redownload is made large enough to be contention-sensitive).
        DO NOT replace this with a plain self.wait_until(...) on
        height_getter alone -- that reintroduces the permanent hang.
        """
        def check():
            if height_getter() >= goal_height:
                return True
            if not self.nodes_linked(node_a, node_b):
                self.log.info(
                    f"nodes[{node_a}]<->nodes[{node_b}] link is down "
                    f"(likely BLOCK_STALLING_TIMEOUT disconnect) while still "
                    f"below goal height {goal_height}; reconnecting"
                )
                self.connect_nodes(node_a, node_b)
            return False

        self.wait_until(check, timeout=timeout, check_interval=5)

    def reorg_back(self):
        # Verify that a block on the old main chain fork has been pruned away
        assert_raises_rpc_error(-1, "Block not available (pruned data)", self.nodes[2].getblock, self.forkhash)
        with self.nodes[2].assert_debug_log(expected_msgs=["Block verification stopping at height", "(no data)"]):
            assert not self.nodes[2].verifychain(checklevel=4, nblocks=0)
        self.log.info(f"Will need to redownload block {self.forkheight}")

        # Verify that we have enough history to reorg back to the fork point
        # Although this is more than 288 blocks, because this chain was written more recently
        # and only its other 299 small and 220 large blocks are in the block files after it,
        # it is expected to still be retained
        self.nodes[2].getblock(self.nodes[2].getblockhash(self.forkheight))

        first_reorg_height = self.nodes[2].getblockcount()
        # BACKPORT (upstream bitcoin/bitcoin PR #35070; not yet in 31.x as of 2026-07-04):
        # invalidate the block at height 1295 (not the mainchain tip) so this test actually
        # exercises the m_blocks_unlinked duplicate-entry path fixed by AddUnlinkedBlock()
        # in node/blockstorage.cpp and validation.cpp. Without that fix, this reliably fails
        # an invariant assert in CheckBlockIndex() when run with -checkblockindex.
        # DO NOT DROP ON NEXT UPSTREAM MERGE/REBASE, and do not revert this back to
        # invalidating self.mainchainheight -- that reverts test coverage for the fix above.
        block_hash_1295 = self.nodes[2].getblockhash(1295)
        self.nodes[2].invalidateblock(block_hash_1295)
        goalbestheight = self.mainchainheight
        goalbesthash = self.mainchainhash2

        # As of 0.10 the current block download logic is not able to reorg to the original chain created in
        # create_chain_with_stale_blocks because it doesn't know of any peer that's on that chain from which to
        # redownload its missing blocks.
        # Invalidate the reorg_test chain in node 0 as well, it can successfully switch to the original chain
        # because it has all the block data.
        # However it must mine enough blocks to have a more work chain than the reorg_test chain in order
        # to trigger node 2's block download logic.
        # At this point node 2 is within 288 blocks of the fork point so it will preserve its ability to reorg
        if self.nodes[2].getblockcount() < self.mainchainheight:
            blocks_to_mine = first_reorg_height + 1 - self.mainchainheight
            self.log.info(f"Rewind node 0 to prev main chain to mine longer chain to trigger redownload. Blocks needed: {blocks_to_mine}")
            self.nodes[0].invalidateblock(block_hash_1295)
            assert_equal(self.nodes[0].getblockcount(), self.mainchainheight)
            assert_equal(self.nodes[0].getbestblockhash(), self.mainchainhash2)
            # FIX: node 0's tip timestamp >> real time; set mocktime on node 0 (to generate)
            # and node 2 (to accept the blocks while waiting below).
            set_mocktime_for_large_blocks([self.nodes[0], self.nodes[2]], blocks_to_mine)
            goalbesthash = self.generate(self.nodes[0], blocks_to_mine, sync_fun=self.no_op)[-1]
            goalbestheight = first_reorg_height + 1

        self.log.info("Verify node 2 reorged back to the main chain, some blocks of which it had to redownload")
        # Wait for Node 2 to reorg to proper height.
        # FIX: use the reconnect-aware wait instead of a plain wait_until --
        # see wait_for_height_with_reconnect() docstring for why. The 233-block
        # redownload triggered above can stall out node0<->node2's manual P2P
        # link under load; a plain wait_until(timeout=N) just hangs until N
        # regardless of how large N is, since nothing ever fetches the peer back.
        self.wait_for_height_with_reconnect(
            node_a=0, node_b=2,
            height_getter=self.nodes[2].getblockcount,
            goal_height=goalbestheight,
            timeout=1200,
        )
        reset_mocktime([self.nodes[0], self.nodes[2]])  # FIX: reset after node 2 has finished reorg
        assert_equal(self.nodes[2].getbestblockhash(), goalbesthash)
        # Verify we can now have the data for a block previously pruned
        assert_equal(self.nodes[2].getblock(self.forkhash)["height"], self.forkheight)

    def manual_test(self, node_number, use_timestamp):
        # at this point, node has 995 blocks and has not yet run in prune mode.
        # Its chain tip timestamp is already > real clock (large blocks with future
        # nTime), so we must pass -mocktime at startup to satisfy the block-DB
        # "block from future" check that dpowcoind performs during initialization.
        tip_time_995 = self.nodes[0].getblock(self.nodes[0].getblockhash(995))["time"]
        self.start_node_mocktime(node_number, tip_time=tip_time_995)
        node = self.nodes[node_number]
        assert_equal(node.getblockcount(), 995)
        assert_raises_rpc_error(-1, "Cannot prune blocks because node is not in prune mode", node.pruneblockchain, 500)

        # now re-start in manual pruning mode - same mocktime protection needed
        self.restart_node_mocktime(node_number, extra_args=["-prune=1"])
        node = self.nodes[node_number]
        assert_equal(node.getblockcount(), 995)

        def height(index):
            if use_timestamp:
                return node.getblockheader(node.getblockhash(index))["time"] + TIMESTAMP_WINDOW
            else:
                return index

        def prune(index):
            ret = node.pruneblockchain(height=height(index))
            assert_equal(ret + 1, node.getblockchaininfo()['pruneheight'])

        def has_block(index):
            return os.path.isfile(os.path.join(self.nodes[node_number].blocks_path, f"blk{index:05}.dat"))

        # should not prune because chain tip of node 3 (995) < PruneAfterHeight (1000)
        assert_raises_rpc_error(-1, "Blockchain is too short for pruning", node.pruneblockchain, height(500))

        # Save block transaction count before pruning, assert value
        block1_details = node.getblock(node.getblockhash(1))
        assert_equal(block1_details["nTx"], len(block1_details["tx"]))

        # mine 6 blocks so we are at height 1001 (i.e., above PruneAfterHeight)
        # FIX: chain tip timestamp >> real time (from 645 large blocks), mocktime required
        set_mocktime_for_large_blocks([node], 6)
        self.generate(node, 6, sync_fun=self.no_op)
        reset_mocktime([node])
        assert_equal(node.getblockchaininfo()["blocks"], 1001)

        # prune parameter in the future (block or timestamp) should raise an exception
        future_parameter = height(1001) + 5
        if use_timestamp:
            assert_raises_rpc_error(-8, "Could not find block with at least the specified timestamp", node.pruneblockchain, future_parameter)
        else:
            assert_raises_rpc_error(-8, "Blockchain is shorter than the attempted prune height", node.pruneblockchain, future_parameter)

        # Pruned block should still know the number of transactions
        assert_equal(node.getblockheader(node.getblockhash(1))["nTx"], block1_details["nTx"])

        # negative heights should raise an exception
        assert_raises_rpc_error(-8, "Negative block height", node.pruneblockchain, -10)

        # height=100 too low to prune first block file so this is a no-op
        prune(100)
        assert has_block(0), "blk00000.dat is missing when should still be there"

        # Does nothing
        node.pruneblockchain(height(0))
        assert has_block(0), "blk00000.dat is missing when should still be there"

        # height=500 should prune first file
        prune(500)
        assert not has_block(0), "blk00000.dat is still there, should be pruned by now"
        assert has_block(1), "blk00001.dat is missing when should still be there"

        # height=650 should prune second file
        prune(650)
        assert not has_block(1), "blk00001.dat is still there, should be pruned by now"

        # height=1000 should not prune anything more, because tip-288 is in blk00002.dat.
        prune(1000)
        assert has_block(2), "blk00002.dat is still there, should be pruned by now"

        # advance the tip so blk00002.dat and blk00003.dat can be pruned (the last 288 blocks should now be in blk00004.dat)
        # FIX: chain tip timestamp >> real time, mocktime required
        set_mocktime_for_large_blocks([node], MIN_BLOCKS_TO_KEEP)
        self.generate(node, MIN_BLOCKS_TO_KEEP, sync_fun=self.no_op)
        reset_mocktime([node])
        prune(1000)
        assert not has_block(2), "blk00002.dat is still there, should be pruned by now"
        assert not has_block(3), "blk00003.dat is still there, should be pruned by now"

        # stop node, start back up with auto-prune at 550 MiB, make sure still runs.
        # FIX: after generate(6) + generate(MIN_BLOCKS_TO_KEEP) the tip timestamp is
        # well ahead of real clock (accumulated mocktime from set_mocktime_for_large_blocks).
        # Pass -mocktime at startup so dpowcoind does not reject its own chain; then clear
        # mocktime immediately via setmocktime(0).
        self.restart_node_mocktime(node_number, extra_args=["-prune=550"])

        self.log.info("Success")

    def test_wallet_rescan(self):
        # check that the pruning node's wallet is still in good shape
        self.log.info("Stop and start pruning node to trigger wallet rescan")
        # FIX: node 2 tip >> real clock after all the reorg/stale-block mining.
        # Use restart_node_mocktime to pass -mocktime at startup, cleared immediately.
        self.restart_node_mocktime(2, extra_args=["-prune=550"])

        # BACKPORT (upstream bitcoin/bitcoin PR #35070, commit 0852925bd8d4; not yet in 31.x
        # as of 2026-07-05): re-fetch getwalletinfo() inside each wait_until predicate instead
        # of checking a snapshot captured once beforehand. The old `wallet_info = ...` pattern
        # froze the dict at call time, so the lambda kept re-checking the same stale values
        # forever -- passing trivially if scanning was already False at capture time, or
        # hanging until timeout if it was True, regardless of the wallet's real progress.
        # DO NOT DROP ON NEXT UPSTREAM MERGE/REBASE.
        self.wait_until(lambda: self.nodes[2].getwalletinfo()["scanning"] == False)
        self.wait_until(lambda: self.nodes[2].getwalletinfo()["lastprocessedblock"]["height"] == self.nodes[2].getblockcount())

        # check that wallet loads successfully when restarting a pruned node after IBD.
        # this was reported to fail in #7494.
        # FIX: restart with -mocktime so startup check accepts the synced chain
        # (tip timestamps >> real clock); cleared via setmocktime(0) right after.
        self.restart_node_mocktime(5, extra_args=["-prune=550", "-blockfilterindex=1"])

        self.wait_until(lambda: self.nodes[5].getwalletinfo()["scanning"] == False)
        self.wait_until(lambda: self.nodes[5].getwalletinfo()["lastprocessedblock"]["height"] == self.nodes[0].getblockcount())

    def run_test(self):
        self.log.info("Warning! This test requires 4GB of disk space")

        self.log.info("Mining a big blockchain of 995 blocks")
        self.create_big_chain()
        # Chain diagram key:
        # *   blocks on main chain
        # +,&,$,@ blocks on other forks
        # X   invalidated block
        # N1  Node 1
        #
        # Start by mining a simple chain that all nodes have
        # N0=N1=N2 **...*(995)

        # stop manual-pruning node with 995 blocks
        self.stop_node(3)
        self.stop_node(4)

        self.log.info("Check that we haven't started pruning yet because we're below PruneAfterHeight")
        self.test_height_min()
        # Extend this chain past the PruneAfterHeight
        # N0=N1=N2 **...*(1020)

        self.log.info("Check that we'll exceed disk space target if we have a very high stale block rate")
        self.create_chain_with_staleblocks()
        # Disconnect N0
        # And mine a 24 block chain on N1 and a separate 25 block chain on N0
        # N1=N2 **...*+...+(1044)
        # N0    **...**...**(1045)
        #
        # reconnect nodes causing reorg on N1 and N2
        # N1=N2 **...*(1020) *...**(1045)
        #                   \
        #                    +...+(1044)
        #
        # repeat this process until you have 12 stale forks hanging off the
        # main chain on N1 and N2
        # N0    *************************...***************************(1320)
        #
        # N1=N2 **...*(1020) *...**(1045) *..         ..**(1295) *...**(1320)
        #                   \            \                      \
        #                    +...+(1044)  &..                    $...$(1319)

        # Save some current chain state for later use
        self.mainchainheight = self.nodes[2].getblockcount()  # 1320
        self.mainchainhash2 = self.nodes[2].getblockhash(self.mainchainheight)

        self.log.info("Check that we can survive a 288 block reorg still")
        self.reorg_test()  # (1033, )
        # Now create a 288 block reorg by mining a longer chain on N1
        # First disconnect N1
        # Then invalidate 1033 on main chain and 1032 on fork so height is 1032 on main chain
        # N1   **...*(1020) **...**(1032)X..
        #                  \
        #                   ++...+(1031)X..
        #
        # Now mine 300 more blocks on N1
        # N1    **...*(1020) **...**(1032) @@...@(1332)
        #                 \               \
        #                  \               X...
        #                   \                 \
        #                    ++...+(1031)X..   ..
        #
        # Reconnect nodes and mine 220 more blocks on N1
        # N1    **...*(1020) **...**(1032) @@...@@@(1552)
        #                 \               \
        #                  \               X...
        #                   \                 \
        #                    ++...+(1031)X..   ..
        #
        # N2    **...*(1020) **...**(1032) @@...@@@(1552)
        #                 \               \
        #                  \               *...**(1320)
        #                   \                 \
        #                    ++...++(1044)     ..
        #
        # N0    ********************(1032) @@...@@@(1552)
        #                                 \
        #                                  *...**(1320)

        self.log.info("Test that we can rerequest a block we previously pruned if needed for a reorg")
        self.reorg_back()
        # Verify that N2 still has block 1033 on current chain (@), but not on main chain (*)
        # Invalidate 1033 on current chain (@) on N2 and we should be able to reorg to
        # original main chain (*), but will require redownload of some blocks
        # In order to have a peer we think we can download from, must also perform this invalidation
        # on N0 and mine a new longest chain to trigger.
        # Final result:
        # N0    ********************(1032) **...****(1553)
        #                                 \
        #                                  X@...@@@(1552)
        #
        # N2    **...*(1020) **...**(1032) **...****(1553)
        #                 \               \
        #                  \               X@...@@@(1552)
        #                   \
        #                    +..
        #
        # N1 doesn't change because 1033 on main chain (*) is invalid

        self.log.info("Test manual pruning with block indices")
        self.manual_test(3, use_timestamp=False)

        self.log.info("Test manual pruning with timestamps")
        self.manual_test(4, use_timestamp=True)

        self.log.info("Syncing node 5 to node 0")
        # FIX: node 5 downloads the full chain from genesis; all those blocks have
        # timestamps >> real time, so mocktime must cover the sync (FTL=600).
        set_mocktime_for_large_blocks([self.nodes[0], self.nodes[5]], 0)
        self.connect_nodes(0, 5)
        self.sync_blocks([self.nodes[0], self.nodes[5]], wait=5, timeout=300)
        reset_mocktime([self.nodes[0], self.nodes[5]])

        if self.is_wallet_compiled():
            self.log.info("Test wallet re-scan")
            self.test_wallet_rescan()

            self.log.info("Test it's not possible to rescan beyond pruned data")
            self.test_rescan_blockchain()

        self.log.info("Test invalid pruning command line options")
        self.test_invalid_command_line_options()

        self.log.info("Test scanblocks can not return pruned data")
        self.test_scanblocks_pruned()

        self.log.info("Test pruneheight reflects the presence of block and undo data")
        self.test_pruneheight_undo_presence()

        self.log.info("Done")

    def test_scanblocks_pruned(self):
        node = self.nodes[5]
        genesis_blockhash = node.getblockhash(0)
        false_positive_spk = bytes.fromhex("001400000000000000000000000000000000000b0619")

        assert genesis_blockhash in node.scanblocks(
            "start", [{"desc": f"raw({false_positive_spk.hex()})"}], 0, 0)['relevant_blocks']

        assert_raises_rpc_error(-1, "Block not available (pruned data)", node.scanblocks,
            "start", [{"desc": f"raw({false_positive_spk.hex()})"}], 0, 0, "basic", {"filter_false_positives": True})

    def test_pruneheight_undo_presence(self):
        node = self.nodes[5]
        pruneheight = node.getblockchaininfo()["pruneheight"]
        fetch_block = node.getblockhash(pruneheight - 1)

        self.connect_nodes(1, 5)
        peers = node.getpeerinfo()
        node.getblockfrompeer(fetch_block, peers[0]["id"])
        self.wait_until(lambda: not try_rpc(-1, "Block not available (pruned data)", node.getblock, fetch_block), timeout=20)

        new_pruneheight = node.getblockchaininfo()["pruneheight"]
        assert_equal(pruneheight, new_pruneheight)

if __name__ == '__main__':
    PruneTest(__file__).main()
