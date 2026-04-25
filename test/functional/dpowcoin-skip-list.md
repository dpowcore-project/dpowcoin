# Dpowcoin functional tests skip-list

The Bitcoin Core functional test suite (under `test/functional/`) is run
unmodified on Dpowcoin where possible. The tests below are inherently
incompatible with Dpowcoin's consensus and are intentionally skipped.

The reasons fall into two categories:

1. **PoW algorithm divergence.** Bitcoin assumes a single SHA256d header
   hash. Dpowcoin requires Yespower **AND** chained Argon2id (BOTH-mode).
   Tests that mine raw blocks via the `MiniNode` framework (which only
   knows SHA256d) cannot satisfy Dpowcoin validation. Tests that assert
   the literal error string `"high-hash"` see `"high-hash-yespower"`
   instead.
2. **Difficulty adjustment divergence.** Bitcoin retargets every 2016
   blocks; Dpowcoin uses LWMA per-block retargeting. Tests that probe
   the 2016-block boundary or `nBits` clamping behave differently.

## Skipped tests

| Test | Category | Reason |
|------|----------|--------|
| `feature_block.py` | PoW | Builds raw blocks expecting SHA256d-only validation. |
| `feature_dersig.py` | PoW + golden hashes | Replays upstream pinned block hashes. |
| `feature_minchainwork.py` | DAA | Assumes fixed-difficulty regtest chain depth. |
| `mining_basic.py` (subset) | PoW | Specific subtests that submit minimally-mined blocks. |
| `p2p_unrequested_blocks.py` | PoW | Crafts orphans validated only against SHA256d. |
| `feature_pruning.py` (timing) | DAA | Relies on 2016-block retarget boundary. |

## Tests that pass unchanged

The bulk of the suite (wallet, mempool, RPC, P2P transport, descriptor
parsing, …) does not touch PoW and runs unmodified on Dpowcoin regtest.

## Maintenance

When porting upstream functional-test additions during future rebases,
classify new tests against this list. If a test is fundamentally tied
to the SHA256d single-PoW model, add it here rather than rewriting it.
