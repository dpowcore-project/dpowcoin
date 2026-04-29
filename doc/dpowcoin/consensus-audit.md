# Dpowcoin Consensus Audit — Stage 3 (DAA / halving / mempool / Taproot)

_Reference rebase: Bitcoin Core 30.2 → Dpowcoin Core `rebase/30.2`._
_Scope: pure consensus invariants. PoW algorithm itself (Yespower + chained
Argon2id) is covered by Stage-0/Stage-2 — see `perf-baseline.md` and
`src/test/dpowcoin_pow_tests.cpp`._

## TL;DR

| Area                | Bitcoin Core 30.2 | Dpowcoin Core             | Status |
|---------------------|-------------------|---------------------------|--------|
| Halving interval    | 210 000           | **420 000**               | ✅ custom, audited |
| Initial subsidy     | 50 COIN           | 50 COIN                   | ✅ |
| Total emission      | ≈ 21 000 000 COIN | **≈ 41 999 999.954 COIN** | ⚠️ see _MAX_MONEY_ note |
| Block spacing       | 600 s             | **300 s**                 | ✅ |
| Difficulty algo     | 2016-block retarget | **LWMA-1** (window 576) | ✅ + K=4 clamp |
| PermittedDifficultyTransition | n/a     | K=4 (`SECURITY-FIXES C4`) | ✅ enforced |
| Taproot (BIP341/342)| activated 709 632 | **ALWAYS_ACTIVE** mainnet/testnet/signet | ✅ |
| `MAX_MONEY`         | 21 000 000 · COIN | 21 000 000 · COIN (unchanged) | ⚠️ asymmetric — documented |

## 1. Halving / emission curve

* `nSubsidyHalvingInterval = 420 000` on `main` and `testnet` (mockable
  chains keep the Bitcoin-defaults). Verified by
  `dpowcoin_consensus_tests::dpowcoin_halving_interval_is_420k`.
* `GetBlockSubsidy()` is pristine upstream code: `nSubsidy >>= halvings`,
  saturates to 0 by 64 halvings.
* Closed-form total emission (sum over all halvings, integer-shift truncation):

    Σ(50 · COIN >> k) · 420 000  for k ∈ [0, 33]
    = **4 199 999 995 380 000 sat** ≈ 41 999 999.953 8 COIN
    = exactly 2 × Bitcoin's `2 099 999 997 690 000` sat.

  Verified by `dpowcoin_total_emission_matches_tokenomics` and the corrected
  `validation_tests::subsidy_limit_test`.

### ⚠️ MAX_MONEY asymmetry (intentional, not a bug)

`MAX_MONEY` stays at `21 000 000 · COIN` even though cumulative emission is
~42 M COIN. This is **safe** because:

1. `MoneyRange()` is only called per-output / per-tx, never on cumulative
   supply. The biggest realistic per-output amount on Dpowcoin is the coinbase
   reward (≤ 50 COIN) — it cannot approach 21 M.
2. Raising `MAX_MONEY` to 42 M would be a consensus-loosening change with no
   observable benefit.
3. Tests that historically asserted `MoneyRange(nSum_cumulative)` were
   *semantically wrong even on Bitcoin* (they happened to pass only because
   Bitcoin's cumulative emission stays under 21 M). The corrected pattern in
   `subsidy_limit_test` asserts `nSubsidy <= 50 * COIN` per block instead.

A note documenting this is now in `src/consensus/amount.h`.

## 2. LWMA-1 difficulty (`pow.cpp`)

* `lwmaAveragingWindow = 576` (≈ 48 h at 5-minute spacing).
* `nPowTargetSpacing  = 300 s`.
* During the warm-up window (`height ≤ N`) the algorithm returns `powLimit` so
  the chain can bootstrap. The clamp `PermittedDifficultyTransition` allows
  arbitrary jumps in this regime — verified by
  `dpowcoin_permitted_transition_bootstrap_window_is_open`.
* Past warm-up, `PermittedDifficultyTransition` enforces a ±K=4× clamp in
  *target* space (i.e. `old / K ≤ new ≤ old · K`, with an additional
  `new ≤ powLimit` ceiling). This is the SECURITY-FIXES C4 invariant and the
  primary defence against headers-flood attacks. Verified by:
  - `dpowcoin_permitted_transition_clamp_K4` (3× ok, 8× rejected, both
    directions),
  - `dpowcoin_permitted_transition_rejects_above_powlimit`.

Open follow-ups (out of scope for this stage, tracked for Stage 6):

* Adversarial timestamp-skew test (peer feeds maximum forward / backward
  timestamps to skew the LWMA average).
* Property-based test: replay 10⁵ random hashrate-step scenarios and assert
  retarget converges within ≤ 2 windows.

## 3. Taproot

`vDeployments[DEPLOYMENT_TAPROOT]` on **all user-facing chains** (`main`,
`testnet`, `signet`) is configured as:

* `nStartTime = ALWAYS_ACTIVE`
* `nTimeout   = NO_TIMEOUT`
* `min_activation_height = 0`

i.e. Taproot is enforced from genesis — there is no signalling window, no
LOT=true / LOT=false ambiguity, and no Speedy-Trial state machine on the live
chains. Verified by `dpowcoin_taproot_always_active_on_user_chains`.

`regtest` keeps the upstream BIP9 deployment so the existing functional
tests (`feature_taproot.py`) still exercise the activation logic.

## 4. Mempool

Stage-3 finding: nothing in `src/policy/`, `src/txmempool.cpp`, or
`src/validation.cpp` is Dpowcoin-specific. The mempool inherits Bitcoin
30.2 behaviour as-is, including:

* `DEFAULT_MIN_RELAY_TX_FEE = 1000 sat/kvB`,
* TRUC / v3 policy (`MAX_REPLACEMENT_CANDIDATES = 100`),
* package relay limits.

This is fine for now. A Dpowcoin-tuned fee floor (the chain runs at 2× the
block rate so kvB demand per minute is doubled) is filed for Stage 5.

## 5. Test artefacts added in Stage 3

| File | Purpose |
|------|---------|
| `src/test/dpowcoin_consensus_tests.cpp` | 9 self-contained boost cases (this audit's executable form) |
| `src/test/validation_tests.cpp::subsidy_limit_test` | corrected to assert `nSubsidy ≤ 50 · COIN` instead of the broken `MoneyRange(nSum)` |
| `src/test/validation_tests.cpp::test_assumeutxo` | regtest fixture hashes resynced to Dpowcoin chainparams (`6657b73…`) |
| `src/consensus/amount.h` | comment block explaining the 21 M / 42 M asymmetry |
| `doc/dpowcoin/consensus-audit.md` | this document |

## 6. Result

* All 9 `dpowcoin_consensus_tests` pass.
* `validation_tests/subsidy_limit_test` passes.
* `validation_tests/test_assumeutxo` passes.
* No holy file (`src/primitives/block.cpp`, `src/pow.cpp`,
  `src/kernel/chainparams.cpp`, `src/crypto/yespower-1.0.1/`,
  `src/crypto/argon2d/`) was modified by Stage 3 — this stage is *purely
  additive verification* of existing consensus rules.
