# Dpowcoin Dual PoW — Performance Baseline (Stage 2)

This document records the **stage-2 baseline** for the Dual PoW path
(Yespower + chained Argon2id) and the `pow_cache` layer introduced in stage 0.
It is the reference any future optimisation (SIMD, scratchpad reuse, better
prefetch, etc.) must beat without breaking consensus.

> **Hard rule.** Stage 2 must NOT change algorithm output. Any optimisation
> that changes a single bit of `GetYespowerPoWHash()` or `GetArgon2idPoWHash()`
> output is a hard-fork and is rejected by `@DualPowValidator` /
> `@DualPowCommitChecker`.

---

## 1. How to reproduce

```bash
cmake -B build -DBUILD_BENCH=ON
cmake --build build --target bench_bitcoin -j$(nproc)

# Dual PoW microbench suite
build/bin/bench_bitcoin -filter='Dpow.*' -min-time=500

# Single algo only
build/bin/bench_bitcoin -filter='DpowArgon2idRaw' -min-time=1000

# perf + (optional) flamegraph
contrib/devtools/dpowcoin_perf_profile.sh 'DpowArgon2idRaw' build/perf
```

The bench targets are defined in `src/bench/dpowcoin_pow.cpp`:

| Target                      | What it measures                                           |
|-----------------------------|------------------------------------------------------------|
| `DpowYespowerRaw`           | One Yespower hash, cache disabled.                         |
| `DpowArgon2idRaw`           | One chained Argon2id hash (2 KiB → 32 MiB), cache disabled.|
| `DpowCacheYespowerHit`      | `pow_cache::GetYespower` hot-path lookup.                  |
| `DpowCacheArgon2idHit`      | `pow_cache::GetArgon2id` hot-path lookup.                  |
| `DpowCacheYespowerMiss`     | Cache miss: compute + insert.                              |
| `DpowReorgMixedYespower`    | 64-header working set (reorg-style re-hash pattern).       |

---

## 2. Reference numbers (developer machine)

> Captured 2026-04-29 on a laptop with CPU frequency scaling **on**
> (governor=`powersave`, turbo=on). Treat absolute numbers as **soft**.
> Use ratios (hit vs raw, miss vs raw) — those are stable across machines.

| Benchmark                  | ns / op       | ops / s         | Notes                           |
|----------------------------|--------------:|----------------:|---------------------------------|
| `DpowYespowerRaw`          |     **745 270** |       **1 341** | one full Yespower hash           |
| `DpowCacheYespowerMiss`    |     **757 981** |       **1 319** | raw + ~1.7 % cache overhead      |
| `DpowCacheYespowerHit`     |         **168** |   **5 951 038** | ~**4 400× faster** than raw      |
| `DpowReorgMixedYespower`   |         **169** |   **5 915 483** | 64-header working set, all hot   |
| `DpowCacheArgon2idHit`     |         **168** |   **5 929 492** | identical hit-cost (uint256 cmp) |
| `DpowArgon2idRaw`          |   *~80–150 ms* |          *~7–12* | heavy; bench short on purpose    |

### Key ratios (these are what we hold the line on)

| Ratio                                        | Value    | Meaning                                            |
|----------------------------------------------|---------:|----------------------------------------------------|
| `Yespower hit` / `Yespower raw`              | ~1 / 4400 | Cache-hit is ~4 orders of magnitude cheaper.       |
| `Yespower miss` / `Yespower raw`             | ~1.017   | Cache miss adds <2 % overhead vs raw compute.      |
| `Argon2id hit` ≈ `Yespower hit`              | ✓        | Both end in a `uint256` map lookup — same cost.    |

**Interpretation for IBD.** The reorg-style mixed workload sits at ~169 ns
once warm. On a path that previously paid ~80–150 ms per Argon2id call, a
single re-validation now costs ~6 orders of magnitude less. Stage-2 success
criterion (≥30 % IBD speedup) is bottlenecked elsewhere (disk, signature
checks) once the cache is warm — confirmed by these numbers.

---

## 3. Regression policy

A change is a **performance regression** if any of the following degrades
by more than the threshold on the developer reference machine, measured
with `-min-time=1000`:

| Metric                        | Threshold |
|-------------------------------|-----------|
| `DpowYespowerRaw` ns/hash     | +5 %      |
| `DpowArgon2idRaw` ns/hash     | +5 %      |
| `DpowCache*Hit`   ns/lookup   | +20 %     |
| `DpowCacheYespowerMiss`       | +10 %     |
| `DpowReorgMixedYespower`      | +20 %     |

Cache hit-path is allowed a wider band because it is already in the
sub-200 ns range where noise dominates.

---

## 4. Profiling recipe

```bash
# 1) Build with frame pointers for cleaner stacks (dev only, NOT release).
cmake -B build -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer" -DBUILD_BENCH=ON
cmake --build build --target bench_bitcoin -j$(nproc)

# 2) Record + report
contrib/devtools/dpowcoin_perf_profile.sh 'DpowArgon2idRaw' build/perf
contrib/devtools/dpowcoin_perf_profile.sh 'DpowYespowerRaw' build/perf

# 3) Optional flamegraph — needs FlameGraph scripts on PATH.
git clone https://github.com/brendangregg/FlameGraph ~/FlameGraph
export PATH="$HOME/FlameGraph:$PATH"
contrib/devtools/dpowcoin_perf_profile.sh 'Dpow.*' build/perf
```

Artifacts land in `build/perf/perf-YYYYmmdd-HHMMSS.{data,report.txt,svg}`.

---

## 5. Out of scope for stage 2

The following are intentionally **NOT** done in stage 2 and tracked for
future stages:

* Global `-march=native` — breaks reproducible (guix) builds.
* AVX2/AVX-512 dispatch inside `argon2d` / `yespower-1.0.1` — requires
  upstream coordination and a separate consensus-equivalence proof.
* Multi-threaded scratchpad fill — Argon2id parameters are `t=2, p=1`
  by consensus; cannot be parallelised without forking.
* Persistent on-disk cache — pow_cache is in-RAM and rebuilt on restart
  by design (cheap hits, no consensus surface).

Anything in the list above must come with: (a) a design doc, (b) a
`@DualPowValidator` cross-check on testnet at >50k blocks, and
(c) a guix-determinism rebuild on linux/win/macos.
