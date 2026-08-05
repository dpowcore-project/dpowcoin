// Copyright (c) 2026-present The Bitweb Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow_cache.h>
#include <random.h>
#include <test/util/random.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <thread>
#include <vector>

/** Test Suite for HeaderPoWCache
 *
 * This suite exercises HeaderPoWCache (pow_cache.h) purely as a cache of
 * uint256 keys -- exactly like cuckoocache_tests.cpp exercises the
 * underlying CuckooCache::cache. Get()/Set() only ever see a bare uint256,
 * so none of these tests construct a CBlockHeader, touch
 * Consensus::Params, or call GetArgon2idPoWHash()/CheckProofOfWork*() --
 * that machinery belongs to the (separately tested) thin wrapper
 * CheckProofOfWorkCached(), not to the cache class itself. Keys here are
 * plain m_rng.rand256() values, following the same "no repeats in the
 * first 200000 calls" property test_cuckoocache_no_fakes relies on.
 *
 * What's covered:
 *  - Get()/Set() basic correctness, and no false positives for keys that
 *    were never Set().
 *  - Filling to (and past) the configured capacity: no false positives
 *    ever appear for keys that were deliberately withheld, regardless of
 *    load factor.
 *  - Reset(): clears prior entries, is safe to call repeatedly, and never
 *    changes the identity of the HeaderPoWCache object itself (relevant
 *    to GetHeaderPoWCache()'s "never returns a dangling reference"
 *    guarantee).
 *  - Concurrency: many threads doing Get()/Set() at once, and a dedicated
 *    thread calling Reset() in a loop concurrently with other threads'
 *    Get()/Set() -- this is a direct check of the "safe to call at any
 *    time ... interleaved with Get()/Set() calls from other threads"
 *    contract documented on HeaderPoWCache::Reset(). Correctness here
 *    means "no crash, no UB, no sanitizer report"; these tests
 *    deliberately do not assert on hit/miss outcomes, since Reset()
 *    racing with Set() makes those outcomes non-deterministic by design.
 */
BOOST_FIXTURE_TEST_SUITE(pow_cache_tests, BasicTestingSetup);

//! Small cache size shared by the single-threaded tests below. Kept small
//! (not the 64 MiB default) so filling to and past capacity is cheap.
static constexpr size_t TEST_CACHE_BYTES{1 << 20}; // 1 MiB == 32768 entries

BOOST_AUTO_TEST_CASE(pow_cache_get_set_basic)
{
    SeedRandomForTest(SeedRand::ZEROS);
    HeaderPoWCache cache{TEST_CACHE_BYTES};

    const uint256 key = m_rng.rand256();
    BOOST_CHECK(!cache.Get(key));
    cache.Set(key);
    BOOST_CHECK(cache.Get(key));
    // Get() is read-only: repeated calls keep returning a hit.
    BOOST_CHECK(cache.Get(key));
    BOOST_CHECK(cache.Get(key));
}

//! No key that was never Set() should ever come back as a hit, no matter
//! how many unrelated keys were inserted first. Mirrors
//! test_cuckoocache_no_fakes in cuckoocache_tests.cpp.
BOOST_AUTO_TEST_CASE(pow_cache_no_fakes)
{
    SeedRandomForTest(SeedRand::ZEROS);
    HeaderPoWCache cache{TEST_CACHE_BYTES};
    for (int x = 0; x < 20000; ++x) {
        cache.Set(m_rng.rand256());
    }
    for (int x = 0; x < 20000; ++x) {
        BOOST_CHECK(!cache.Get(m_rng.rand256()));
    }
}

//! Filling well past the cache's configured capacity must never produce a
//! false positive for a key that was deliberately withheld. Only the
//! even-indexed half of a large key set is inserted; the odd-indexed half
//! is checked and must always report a miss, at every point during and
//! after the fill.
BOOST_AUTO_TEST_CASE(pow_cache_overfill_no_false_positive)
{
    SeedRandomForTest(SeedRand::ZEROS);
    HeaderPoWCache cache{TEST_CACHE_BYTES};

    const size_t capacity_entries = TEST_CACHE_BYTES / sizeof(uint256);
    // 4x the cache's capacity, so this comfortably exercises eviction.
    const size_t n = capacity_entries * 4;

    std::vector<uint256> keys;
    keys.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        keys.push_back(m_rng.rand256());
    }

    for (size_t i = 0; i < n; ++i) {
        if (i % 2 == 0) {
            cache.Set(keys[i]);
        }
        // Withheld (odd-indexed) keys must never be reported as present,
        // checked at every step of the fill, not just at the end.
        if (i > 0 && (i - 1) % 2 != 0) {
            BOOST_CHECK(!cache.Get(keys[i - 1]));
        }
    }
    // Final sweep over every withheld key.
    for (size_t i = 1; i < n; i += 2) {
        BOOST_CHECK(!cache.Get(keys[i]));
    }
}

//! Filling to (not past) capacity should give a hit rate close to 100%,
//! same expectation cuckoocache_hit_rate_ok has of the underlying
//! CuckooCache::cache at load <= 1.0.
BOOST_AUTO_TEST_CASE(pow_cache_hit_rate_at_capacity)
{
    SeedRandomForTest(SeedRand::ZEROS);
    HeaderPoWCache cache{TEST_CACHE_BYTES};

    const size_t capacity_entries = TEST_CACHE_BYTES / sizeof(uint256);
    // Load factor 0.9: comfortably under capacity, where a healthy cuckoo
    // table should retain nearly everything.
    const size_t n = static_cast<size_t>(capacity_entries * 0.9);

    std::vector<uint256> keys;
    keys.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        keys.push_back(m_rng.rand256());
    }
    for (const uint256& k : keys) {
        cache.Set(k);
    }
    size_t hits = 0;
    for (const uint256& k : keys) {
        hits += cache.Get(k);
    }
    BOOST_CHECK(double(hits) / double(n) > 0.98);
}

//! Reset() must discard everything inserted so far.
BOOST_AUTO_TEST_CASE(pow_cache_reset_clears_entries)
{
    SeedRandomForTest(SeedRand::ZEROS);
    HeaderPoWCache cache{TEST_CACHE_BYTES};

    std::vector<uint256> keys;
    for (int i = 0; i < 1000; ++i) {
        keys.push_back(m_rng.rand256());
        cache.Set(keys.back());
    }
    for (const uint256& k : keys) {
        BOOST_CHECK(cache.Get(k));
    }

    cache.Reset(TEST_CACHE_BYTES);

    for (const uint256& k : keys) {
        BOOST_CHECK(!cache.Get(k));
    }
    // The cache remains fully usable after Reset(): new entries can still
    // be inserted and found.
    const uint256 fresh = m_rng.rand256();
    BOOST_CHECK(!cache.Get(fresh));
    cache.Set(fresh);
    BOOST_CHECK(cache.Get(fresh));
}

//! Reset() is safe to call repeatedly, back-to-back, including at a
//! different size than the cache was constructed with.
BOOST_AUTO_TEST_CASE(pow_cache_reset_repeated_and_resizes)
{
    HeaderPoWCache cache{TEST_CACHE_BYTES};
    for (int i = 0; i < 10; ++i) {
        cache.Reset(TEST_CACHE_BYTES);
        cache.Reset(TEST_CACHE_BYTES * 2);
        cache.Reset(TEST_CACHE_BYTES / 2);
    }
    // Still usable after repeated resets.
    SeedRandomForTest(SeedRand::ZEROS);
    const uint256 key = m_rng.rand256();
    cache.Set(key);
    BOOST_CHECK(cache.Get(key));
}

//! Reset() never replaces the HeaderPoWCache object itself -- only its
//! internal contents. This is what makes GetHeaderPoWCache()'s returned
//! reference safe to hold across a Reset() from elsewhere: the object's
//! address must stay stable.
BOOST_AUTO_TEST_CASE(pow_cache_reset_preserves_object_identity)
{
    HeaderPoWCache cache{TEST_CACHE_BYTES};
    const HeaderPoWCache* const addr_before = &cache;
    cache.Reset(TEST_CACHE_BYTES);
    const HeaderPoWCache* const addr_after = &cache;
    BOOST_CHECK_EQUAL(addr_before, addr_after);
}

//! GetHeaderPoWCache() itself: same object across calls, and its address
//! survives an InitHeaderPoWCache() call (which Resets it under the
//! hood).
BOOST_AUTO_TEST_CASE(pow_cache_global_instance_identity_survives_init)
{
    HeaderPoWCache& before = GetHeaderPoWCache();
    const HeaderPoWCache* const addr_before = &before;

    InitHeaderPoWCache(TEST_CACHE_BYTES);

    HeaderPoWCache& after = GetHeaderPoWCache();
    BOOST_CHECK_EQUAL(addr_before, &after);
    // Restore the default so this test doesn't leave global state behind
    // for whichever test runs next in the same process.
    InitHeaderPoWCache(DEFAULT_HEADER_POW_CACHE_BYTES);
}

//! Many threads hammering Get()/Set() concurrently on the same cache:
//! must produce no crash and no sanitizer-detected UB. Modeled on
//! cuckoocache_erase_parallel_ok in cuckoocache_tests.cpp.
BOOST_AUTO_TEST_CASE(pow_cache_concurrent_get_set)
{
    HeaderPoWCache cache{TEST_CACHE_BYTES};
    constexpr int num_threads = 8;
    constexpr int ops_per_thread = 20000;

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&cache] {
            FastRandomContext rng{true};
            for (int i = 0; i < ops_per_thread; ++i) {
                const uint256 key = rng.rand256();
                if (i % 2 == 0) {
                    cache.Set(key);
                } else {
                    (void)cache.Get(key);
                }
            }
        });
    }
    for (std::thread& th : threads) {
        th.join();
    }
}

//! Direct check of the documented contract on HeaderPoWCache::Reset():
//! "safe to call at any time ... interleaved with Get()/Set() calls from
//! other threads". One thread calls Reset() in a loop while several
//! others concurrently call Get()/Set(); this must never crash or trip a
//! sanitizer, and Get()/Set() must remain well-defined (a hit is always
//! either "not yet reset away" or "reinserted after a reset", never
//! garbage) throughout.
BOOST_AUTO_TEST_CASE(pow_cache_reset_interleaved_with_get_set)
{
    HeaderPoWCache cache{TEST_CACHE_BYTES};
    constexpr int num_worker_threads = 6;
    constexpr int ops_per_thread = 5000;
    constexpr int reset_iterations = 200;

    std::vector<std::thread> threads;
    threads.reserve(num_worker_threads + 1);

    // Dedicated thread that resets the cache in a loop throughout the
    // whole test.
    threads.emplace_back([&cache] {
        for (int i = 0; i < reset_iterations; ++i) {
            cache.Reset(TEST_CACHE_BYTES);
        }
    });

    // Worker threads doing ordinary Get()/Set() traffic concurrently with
    // the resets above.
    for (int t = 0; t < num_worker_threads; ++t) {
        threads.emplace_back([&cache] {
            FastRandomContext rng{true};
            std::vector<uint256> local_keys;
            local_keys.reserve(ops_per_thread);
            for (int i = 0; i < ops_per_thread; ++i) {
                const uint256 key = rng.rand256();
                local_keys.push_back(key);
                cache.Set(key);
                // A Get() right after a Set() may legitimately miss if a
                // concurrent Reset() lands in between -- that's expected
                // and not checked here. What matters is that the call
                // itself is race-free (caught by TSAN/ASAN, not by an
                // assertion on the boolean result).
                (void)cache.Get(key);
                if (i > 0) {
                    (void)cache.Get(local_keys[i - 1]);
                }
            }
        });
    }

    for (std::thread& th : threads) {
        th.join();
    }
}

BOOST_AUTO_TEST_SUITE_END();
