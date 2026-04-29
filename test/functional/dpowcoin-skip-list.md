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
| `feature_cltv.py` | PoW + golden hashes | Same as `feature_dersig.py` — pinned bitcoin testnet hashes. |
| `feature_signet.py` | Network | Dpowcoin does not ship a signet; bitcoin testnet4/BIP94 assumptions. |
| `feature_minchainwork.py` | DAA | Assumes fixed-difficulty regtest chain depth (2016-block window). |
| `feature_pruning.py` | DAA + timing | Relies on Bitcoin's 2016-block retarget boundary. |
| `mining_basic.py` | PoW | Asserts on `"high-hash"` literal; Dpowcoin emits `"high-hash-yespower"` / `"-argon2id"`. |
| `mining_template_verification.py` | PoW | Submits MiniNode-mined templates that fail Dual PoW. |
| `interface_http.py` | PoW | Embeds expected error strings tied to single-PoW path. |
| `p2p_unrequested_blocks.py` | PoW | Crafts orphans validated only against SHA256d. |

## Tests that pass unchanged

The bulk of the suite (wallet, mempool, RPC, P2P transport, descriptor
parsing, …) does not touch PoW and runs unmodified on Dpowcoin regtest.

## Maintenance

When porting upstream functional-test additions during future rebases,
classify new tests against this list. If a test is fundamentally tied
to the SHA256d single-PoW model, add it here rather than rewriting it.
