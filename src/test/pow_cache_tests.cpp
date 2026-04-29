// Copyright (c) 2026 The Dpowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow_cache.h>
#include <primitives/block.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <atomic>
#include <thread>
#include <vector>

#include <boost/test/unit_test.hpp>

//! Coverage for src/pow_cache.{h,cpp}.
//!
//! The cache is a transparent LRU memoization layer over
//! CBlockHeader::Get{Yespower,Argon2id}PoWHash(). It must:
//!  1) return values that are bit-identical to recomputation,
//!  2) record hits on repeat lookups of the same header,
//!  3) evict the oldest entry when capacity is exceeded,
//!  4) be effectively disabled when capacity is set to 0,
//!  5) survive Init/Shutdown cycles without leaking previous state.

BOOST_FIXTURE_TEST_SUITE(pow_cache_tests, BasicTestingSetup)

namespace {

//! Build a deterministic header. nNonce parameterizes a unique 80-byte
//! payload so each call yields a distinct GetHash() (the cache key).
CBlockHeader MakeHeader(uint32_t nonce)
{
    CBlockHeader h;
    h.nVersion       = 1;
    h.hashPrevBlock  = uint256();
    h.hashMerkleRoot = uint256();
    h.nTime          = 1296688602;
    h.nBits          = 0x207fffff;
    h.nNonce         = nonce;
    return h;
}

} // namespace

//! 1) Cached value must equal recomputation (no consensus drift).
BOOST_AUTO_TEST_CASE(cache_returns_identical_hash_to_recompute)
{
    pow_cache::Init(128);
    CBlockHeader h = MakeHeader(2);

    const uint256 direct_y = h.GetYespowerPoWHash();
    const uint256 direct_a = h.GetArgon2idPoWHash();

    const uint256 cached_y = pow_cache::GetYespower(h);
    const uint256 cached_a = pow_cache::GetArgon2id(h);

    BOOST_CHECK_EQUAL(cached_y.GetHex(), direct_y.GetHex());
    BOOST_CHECK_EQUAL(cached_a.GetHex(), direct_a.GetHex());

    pow_cache::Shutdown();
}

//! 2) Repeat lookup of the same header registers as a hit; first as a miss.
BOOST_AUTO_TEST_CASE(repeat_lookup_is_a_hit)
{
    pow_cache::Init(128);
    CBlockHeader h = MakeHeader(3);

    // First call: miss + insert.
    (void)pow_cache::GetYespower(h);
    auto s1 = pow_cache::GetYespowerStats();
    BOOST_CHECK_EQUAL(s1.misses, 1u);
    BOOST_CHECK_EQUAL(s1.hits,   0u);
    BOOST_CHECK_EQUAL(s1.size,   1u);

    // Second call: hit.
    (void)pow_cache::GetYespower(h);
    auto s2 = pow_cache::GetYespowerStats();
    BOOST_CHECK_EQUAL(s2.misses, 1u);
    BOOST_CHECK_EQUAL(s2.hits,   1u);
    BOOST_CHECK_EQUAL(s2.size,   1u);

    pow_cache::Shutdown();
}

//! 3) Capacity is honored: insertion past capacity triggers LRU eviction.
//!    We use the cheaper Yespower side to keep the test fast.
BOOST_AUTO_TEST_CASE(lru_evicts_oldest_when_capacity_exceeded)
{
    pow_cache::Init(2);

    CBlockHeader a = MakeHeader(10);
    CBlockHeader b = MakeHeader(11);
    CBlockHeader c = MakeHeader(12);

    (void)pow_cache::GetYespower(a); // [a]
    (void)pow_cache::GetYespower(b); // [b,a]
    (void)pow_cache::GetYespower(c); // [c,b], a evicted

    auto s = pow_cache::GetYespowerStats();
    BOOST_CHECK_EQUAL(s.capacity,  2u);
    BOOST_CHECK_EQUAL(s.size,      2u);
    BOOST_CHECK_GE   (s.evictions, 1u);

    // Re-querying `a` should now be a miss again (it was evicted).
    const uint64_t misses_before = s.misses;
    (void)pow_cache::GetYespower(a);
    auto s2 = pow_cache::GetYespowerStats();
    BOOST_CHECK_EQUAL(s2.misses, misses_before + 1);

    pow_cache::Shutdown();
}

//! 4) capacity == 0 disables the cache: every call is a miss, size stays 0,
//!    but the returned hash is still correct.
BOOST_AUTO_TEST_CASE(zero_capacity_disables_cache_but_keeps_correctness)
{
    pow_cache::Init(0);
    CBlockHeader h = MakeHeader(7);

    const uint256 direct = h.GetYespowerPoWHash();
    const uint256 v1     = pow_cache::GetYespower(h);
    const uint256 v2     = pow_cache::GetYespower(h);

    BOOST_CHECK_EQUAL(v1.GetHex(), direct.GetHex());
    BOOST_CHECK_EQUAL(v2.GetHex(), direct.GetHex());

    auto s = pow_cache::GetYespowerStats();
    BOOST_CHECK_EQUAL(s.capacity, 0u);
    BOOST_CHECK_EQUAL(s.size,     0u);
    BOOST_CHECK_EQUAL(s.hits,     0u);
    BOOST_CHECK_GE   (s.misses,   2u);

    pow_cache::Shutdown();
}

//! 5) Shutdown clears state; a subsequent Init starts from a clean slate.
BOOST_AUTO_TEST_CASE(shutdown_then_reinit_resets_state)
{
    pow_cache::Init(64);
    CBlockHeader h = MakeHeader(99);
    (void)pow_cache::GetYespower(h);
    BOOST_CHECK_EQUAL(pow_cache::GetYespowerStats().size, 1u);

    pow_cache::Shutdown();
    auto sd = pow_cache::GetYespowerStats();
    BOOST_CHECK_EQUAL(sd.size,     0u);
    BOOST_CHECK_EQUAL(sd.capacity, 0u);

    pow_cache::Init(64);
    auto si = pow_cache::GetYespowerStats();
    BOOST_CHECK_EQUAL(si.size,     0u);
    BOOST_CHECK_EQUAL(si.capacity, 64u);

    pow_cache::Shutdown();
}

//! 6) Concurrent Lookup/Insert from many threads must not race or corrupt
//!    state, and must yield correct results. Yespower is fast enough to
//!    keep the test under a second on CI.
BOOST_AUTO_TEST_CASE(concurrent_lookup_is_thread_safe)
{
    pow_cache::Init(64);

    constexpr int kThreads = 8;
    constexpr int kIters   = 50;

    // Pre-build a small fixed working set so threads collide on keys.
    std::vector<CBlockHeader> headers;
    headers.reserve(8);
    for (uint32_t i = 0; i < 8; ++i) headers.push_back(MakeHeader(1000 + i));

    // Reference values computed single-threaded.
    std::vector<uint256> ref;
    ref.reserve(headers.size());
    for (auto& h : headers) ref.push_back(h.GetYespowerPoWHash());

    std::atomic<int> mismatches{0};
    std::vector<std::thread> ts;
    ts.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([&, t] {
            for (int i = 0; i < kIters; ++i) {
                const size_t idx = (t + i) % headers.size();
                const uint256 v = pow_cache::GetYespower(headers[idx]);
                if (v != ref[idx]) ++mismatches;
            }
        });
    }
    for (auto& th : ts) th.join();

    BOOST_CHECK_EQUAL(mismatches.load(), 0);

    auto s = pow_cache::GetYespowerStats();
    // Total accesses == hits + misses, and size never exceeds capacity.
    BOOST_CHECK_EQUAL(s.hits + s.misses, static_cast<uint64_t>(kThreads * kIters));
    BOOST_CHECK_LE(s.size, s.capacity);

    pow_cache::Shutdown();
}

BOOST_AUTO_TEST_SUITE_END()
