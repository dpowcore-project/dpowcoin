// Copyright (c) 2026 The Dpowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <pow_cache.h>
#include <primitives/block.h>
#include <random.h>
#include <uint256.h>

#include <array>
#include <cstdint>
#include <vector>

//! Stage-2 microbenchmarks for the Dpowcoin Dual PoW path.
//!
//! What we measure:
//!   * Raw Yespower throughput (header → 32 B hash) — the cheap algo.
//!   * Raw chained Argon2id throughput — the heavy algo (32 MiB scratchpad).
//!   * pow_cache hit path latency (memory-only lookup, no compute).
//!   * pow_cache miss path latency (compute + insert).
//!
//! Why: Stage-2 success criteria require a measurable IBD speedup. The cache
//! must amortize Argon2id cost on reorgs / re-checks. These benches give us
//! the per-call numbers needed to project IBD impact and to detect any
//! regression introduced by future SIMD / refactor work.
//!
//! The header is built from a fixed deterministic seed so results are stable
//! across runs and across machines for relative comparison.

namespace {

CBlockHeader MakeDeterministicHeader(uint32_t nonce_seed)
{
    CBlockHeader h;
    h.nVersion       = 1;
    // Use a stable, non-trivial prev-hash so we don't accidentally bench an
    // all-zero input that some implementations short-circuit.
    std::array<uint8_t, 32> prev{};
    for (size_t i = 0; i < prev.size(); ++i) prev[i] = static_cast<uint8_t>(i ^ 0x5A);
    h.hashPrevBlock  = uint256(prev);
    std::array<uint8_t, 32> mr{};
    for (size_t i = 0; i < mr.size(); ++i) mr[i] = static_cast<uint8_t>((i * 7) & 0xFF);
    h.hashMerkleRoot = uint256(mr);
    h.nTime          = 1714000000;
    h.nBits          = 0x207fffff;
    h.nNonce         = nonce_seed;
    return h;
}

} // namespace

//
// ---- Raw algorithm throughput ----
//

static void DpowYespowerRaw(benchmark::Bench& bench)
{
    // Disable cache entirely so each call hits the algorithm.
    pow_cache::Init(0);
    CBlockHeader h = MakeDeterministicHeader(0);
    uint32_t nonce = 0;
    bench.unit("hash").run([&] {
        h.nNonce = nonce++;
        uint256 r = h.GetYespowerPoWHash();
        ankerl::nanobench::doNotOptimizeAway(r);
    });
    pow_cache::Shutdown();
}

static void DpowArgon2idRaw(benchmark::Bench& bench)
{
    // Argon2id is heavy (~80–150 ms per call). Keep the bench short.
    pow_cache::Init(0);
    CBlockHeader h = MakeDeterministicHeader(0);
    uint32_t nonce = 0;
    bench.minEpochIterations(3).unit("hash").run([&] {
        h.nNonce = nonce++;
        uint256 r = h.GetArgon2idPoWHash();
        ankerl::nanobench::doNotOptimizeAway(r);
    });
    pow_cache::Shutdown();
}

//
// ---- pow_cache hit / miss latency ----
//

static void DpowCacheYespowerHit(benchmark::Bench& bench)
{
    pow_cache::Init(pow_cache::DEFAULT_CAPACITY);
    CBlockHeader h = MakeDeterministicHeader(42);
    // Warm.
    (void)pow_cache::GetYespower(h);
    bench.unit("lookup").run([&] {
        uint256 r = pow_cache::GetYespower(h);
        ankerl::nanobench::doNotOptimizeAway(r);
    });
    pow_cache::Shutdown();
}

static void DpowCacheArgon2idHit(benchmark::Bench& bench)
{
    pow_cache::Init(pow_cache::DEFAULT_CAPACITY);
    CBlockHeader h = MakeDeterministicHeader(43);
    // Warm (single heavy call).
    (void)pow_cache::GetArgon2id(h);
    bench.unit("lookup").run([&] {
        uint256 r = pow_cache::GetArgon2id(h);
        ankerl::nanobench::doNotOptimizeAway(r);
    });
    pow_cache::Shutdown();
}

static void DpowCacheYespowerMiss(benchmark::Bench& bench)
{
    pow_cache::Init(pow_cache::DEFAULT_CAPACITY);
    uint32_t nonce = 1000;
    bench.unit("miss").run([&] {
        CBlockHeader h = MakeDeterministicHeader(nonce++);
        uint256 r = pow_cache::GetYespower(h);
        ankerl::nanobench::doNotOptimizeAway(r);
    });
    pow_cache::Shutdown();
}

//
// ---- Reorg-style mixed workload ----
//
// Models the IBD/reorg pattern: a small working-set of headers gets re-hashed
// many times. Cache should make this nearly free for Argon2id once warm.
//
static void DpowReorgMixedYespower(benchmark::Bench& bench)
{
    pow_cache::Init(pow_cache::DEFAULT_CAPACITY);
    constexpr int kHeaders = 64;
    std::vector<CBlockHeader> headers;
    headers.reserve(kHeaders);
    for (int i = 0; i < kHeaders; ++i) headers.push_back(MakeDeterministicHeader(7000 + i));
    // Warm the cache.
    for (auto& h : headers) (void)pow_cache::GetYespower(h);

    size_t idx = 0;
    bench.unit("mixed_lookup").run([&] {
        const CBlockHeader& h = headers[idx];
        idx = (idx + 1) % headers.size();
        uint256 r = pow_cache::GetYespower(h);
        ankerl::nanobench::doNotOptimizeAway(r);
    });
    pow_cache::Shutdown();
}

BENCHMARK(DpowYespowerRaw,         benchmark::PriorityLevel::HIGH);
BENCHMARK(DpowArgon2idRaw,         benchmark::PriorityLevel::HIGH);
BENCHMARK(DpowCacheYespowerHit,    benchmark::PriorityLevel::HIGH);
BENCHMARK(DpowCacheArgon2idHit,    benchmark::PriorityLevel::HIGH);
BENCHMARK(DpowCacheYespowerMiss,   benchmark::PriorityLevel::HIGH);
BENCHMARK(DpowReorgMixedYespower,  benchmark::PriorityLevel::HIGH);
