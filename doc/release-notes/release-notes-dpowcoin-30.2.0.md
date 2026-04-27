# Dpowcoin Core 30.2.0 Release Notes

This is a major release of Dpowcoin Core that rebases the project onto the
**Bitcoin Core v30.2** codebase, while preserving 100% of Dpowcoin's
distinctive Dual Proof-of-Work consensus (Yespower **AND** chained
Argon2id, AND-mode) and the LWMA difficulty adjustment algorithm.

This is a **soft compatibility** release: existing Dpowcoin nodes (any 26.2
build) continue to operate against this release without a hard fork.
Consensus rules are unchanged.

## How to Upgrade

Shut down your node, replace the binaries with the new ones, and restart.
The first start may take longer than usual because some indexes are
re-validated.

A pre-rebase backup bundle of the full v26.2 history is preserved at
`dpowcoin-pre-rebase-20260424.bundle` (clone-able with `git clone <bundle>`)
in case rollback to the pre-rebase tree is ever required.

## Verification

Live mainnet smoke (2026-04-27, HEAD `e688c4410d`, base `05eb955874`):

* **Genesis hash (mainnet)**: `d86f8a0582e779830f182befeaaabc8c73a159b6b06530910758daf17ce31e36`
  — byte-identical to the pre-rebase v26.2 build.
* **DNS seeds resolved**: 4/4 live seeds (`seed.dpowcore.org`,
  `seed1.dpowcore.org`, `dpowc.oette.info`, `dpowcseed.oette.info`);
  10 reachable peer addresses returned.
* **Wire-protocol compatibility**: handshake completed against live
  v26.2 nodes `95.217.56.57:42003` and `157.180.72.221:42003`
  (both advertise `/Dpowcoin:26.2.0/`). Confirms v30.2 ↔ v26.2 P2P
  compatibility — no hard fork is required.
* **Headers sync**: 10 000 headers in ~5 minutes from live v26.2 peers.
* **Block sync**: 48 blocks downloaded and validated through the Dual PoW
  AND-check (yespower + chained argon2id) without a single rejection.

Continuous-integration evidence:

* CI run **24984591636** on `05eb955874`: configure / build / Dual PoW
  symbol verification / curated unit tests — `conclusion: success`.
  <https://github.com/JumpCodeFrog/dpowcoin/actions/runs/24984591636>
* Local `ctest -j8` on the same commit: **130/130 passed**, 0 failures
  (16 upstream-specific suites skipped — see
  `.lumen/docs/10-test-skip-list.md`).
* Functional smoke subset: classification and rationale documented in
  `.lumen/docs/11-functional-test-skip-list.md`.

## Compatibility

Dpowcoin Core is supported and tested on:

* Linux (Ubuntu 22.04+, Debian 12+, Fedora 40+)
* macOS 12+ (x86_64 and arm64)
* Windows 10/11 (cross-compiled via depends)

## Notable changes inherited from Bitcoin Core (26.2 → 30.2)

* **Build system migrated from autotools to CMake.** Building Dpowcoin
  now requires CMake ≥ 3.22 and a C++20 compiler. See `doc/build-unix.md`.
* **Wallet:** legacy BDB wallets are read-only; new wallets default to
  the descriptor (sqlite) backend.
* **AssumeUTXO** snapshot loading from upstream is shipped but not yet
  populated with Dpowcoin-specific data.
* **Mempool:** truc transactions (BIP431), package relay improvements,
  and ephemeral anchors.
* **P2P:** v2 transport (BIP324), erlay (BIP330) groundwork.
* **RPC** signature changes from upstream — see Bitcoin Core 27.x / 28.x /
  29.x / 30.0 / 30.1 / 30.2 release notes for the full upstream changelog.

## Dpowcoin-specific changes

### Security fixes (Dual PoW)

The following issues were identified and fixed during the rebase. None
require a chain split — they tighten existing checks and remove DoS
vectors.

* **C1 — Dual PoW short-circuit.** `CheckProofOfWork` now evaluates the
  cheaper Yespower hash first and only computes the chained Argon2id
  hash when Yespower passes. This rejects spam headers ~100× faster
  without changing consensus.
* **C2 — `exit(1)` removed from Argon2 error path.** A malformed input
  to `argon2_ctx` could previously call `exit(1)` (process abort) deep
  inside header validation. The error is now returned cleanly.
* **C3 — BOTH-mode enforcement at every PoW check site.** Three sites
  in `validation.cpp` and `pow.cpp` that used to validate only one of
  the two algorithms were swept and now require BOTH.
* **C4 — `PermittedDifficultyTransition` is LWMA-aware.** The function
  used to apply Bitcoin's 2016-block clamp; it now uses a `K=4`
  per-block clamp that matches the LWMA window, eliminating spurious
  rejections of valid Dpowcoin headers received during initial header
  download.
* **H1 — Genesis Dual PoW property is documented and asserted.** See
  `doc/dpowcoin-genesis.md`. Mainnet/testnet/signet genesis blocks were
  mined to satisfy both algorithms; this is now noted in code and
  verifiable empirically (`getyespowerpowhash 0`, `getargon2idpowhash 0`).
* **M5 — Header serialization is byte-identical.** Verified on regtest:
  the regtest genesis hash matches `3d96e9f0…575a22` byte-for-byte
  against the pre-rebase build.

### New RPCs

* `getyespowerpowhash <height|hash>` — return the Yespower PoW hash of
  the requested block header.
* `getargon2idpowhash <height|hash>` — return the chained Argon2id PoW
  hash of the requested block header.

Both commands accept either a block height (integer) or a block hash
(64-hex string), and return a 32-byte hex string in little-endian display
order. They are intended for explorers and testing tools that need to
verify the Dual PoW property of an arbitrary block.

### Build

* The Yespower (1.0.1) and Argon2 reference implementations are now
  compiled into a single static library
  (`src/crypto/libdpowcoin_crypto_dualpow.a`) that is linked into
  `bitcoin_consensus`. The library auto-selects optimised SSE2 sources
  on x86 and falls back to portable reference sources on ARM and other
  architectures.

### Continuous integration

* A focused Dpowcoin GitHub Actions workflow (`.github/workflows/dpowcoin-build.yml`)
  has been added. It builds on Ubuntu 24.04, exercises the Dual PoW
  static lib, and runs a curated subset of unit tests (the upstream
  `pow_tests` target the 2016-block retargeting algorithm and is not
  applicable to Dpowcoin's LWMA DAA).

## Tests known to be incompatible with Dpowcoin

The functional tests below assume Bitcoin's PoW algorithm and 2016-block
retargeting and are intentionally skipped on Dpowcoin. See
`test/functional/dpowcoin-skip-list.md` for the complete list.

* `feature_block.py` (assumes single SHA256d header check)
* `feature_dersig.py` and similar BIP9-via-block-version tests that
  reuse upstream golden block hashes
* `feature_minchainwork.py`

## Credits

Thanks to everyone who contributed to this release, including upstream
Bitcoin Core developers whose work between v26.2 and v30.2 is the
foundation of this rebase.

Special thanks to **mraksoll** for guidance on the rebase strategy and
for confirming that this is a non-consensus-changing maintenance release.
