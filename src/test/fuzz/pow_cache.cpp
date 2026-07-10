// Copyright (c) 2026-present The Dpowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow_cache.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <test/fuzz/util.h>
#include <uint256.h>
#include <util/hasher.h>

#include <cassert>
#include <cstdint>
#include <unordered_set>

// This target fuzzes HeaderPoWCache (pow_cache.h) as a standalone cache of
// uint256 keys -- the same "test/fuzz code construct independent local
// instances directly" pattern the class's doc comment calls out, and the
// same scope as cuckoocache.cpp's fuzz target for the underlying
// CuckooCache::cache. It never constructs a CBlockHeader or touches
// Consensus::Params/GetArgon2idPoWHash()/CheckProofOfWork(); the wrapper
// CheckProofOfWorkCached() is a separate, thin piece of logic that belongs
// to its own (much smaller-scale) differential test, not to this one.
//
// [Dpowcoin] Ported from bitweb 30.x's src/test/fuzz/pow_cache.cpp, with
// two adaptations for this tree:
//  - No SeedRandomStateForTest() call: that helper doesn't exist in this
//    tree's test/util/random.h, and this target has no dependency on the
//    global insecure-rand state to begin with -- every key comes from
//    fuzzed_data_provider via ConsumeUInt256(), same as this tree's own
//    test/fuzz/cuckoocache.cpp target, which likewise seeds nothing.
//  - SaltedTxidHasher (util/hasher.h) in place of upstream's
//    SaltedUint256Hasher -- this tree hasn't picked up that rename yet,
//    but SaltedTxidHasher's operator()(const uint256&) is the identical
//    interface, so it's a drop-in substitute for hashing uint256 keys in
//    the shadow set below.
//
// A local std::unordered_set<uint256> shadows "every key Set() since the
// last Reset()" and acts as the oracle: for any key not in that set,
// cache.Get() must be false. A miss for a key that *is* in the shadow set
// is an allowed eviction (false negative) -- CuckooCache is a lossy cache
// by design -- but a hit for a key that was never Set() (a false positive)
// is never acceptable and would indicate a real bug in HeaderPoWCache.
FUZZ_TARGET(pow_cache)
{
    FuzzedDataProvider fuzzed_data_provider(buffer.data(), buffer.size());

    const size_t megabytes = fuzzed_data_provider.ConsumeIntegralInRange<size_t>(0, 16);
    HeaderPoWCache cache{megabytes << 20};
    std::unordered_set<uint256, SaltedTxidHasher> shadow_inserted;

    LIMITED_WHILE(fuzzed_data_provider.ConsumeBool(), 10000)
    {
        // 0 = Set, 1 = Get, 2 = Reset
        switch (fuzzed_data_provider.ConsumeIntegralInRange<int>(0, 2)) {
        case 0: {
            const uint256 key = ConsumeUInt256(fuzzed_data_provider);
            cache.Set(key);
            shadow_inserted.insert(key);
            break;
        }
        case 1: {
            const uint256 key = ConsumeUInt256(fuzzed_data_provider);
            const bool hit = cache.Get(key);
            // [Dpowcoin] std::unordered_set::contains() is C++20-only; this
            // tree builds in C++17 mode by default (configure.ac's
            // --enable-c++20 is off unless explicitly passed), so use
            // count() instead -- identical result for a set (0 or 1), just
            // available since C++11.
            if (!shadow_inserted.count(key) && key != uint256{}) {
                // Never Set() since the last Reset(): a hit here would be
                // a false positive, which must never happen -- except for
                // the all-zero key. CuckooCache::cache's table is
                // value-initialized on setup()/Reset(), so every not-yet-
                // occupied slot already holds a default-constructed
                // Element, i.e. uint256{} (see base_blob's constexpr
                // default constructor in uint256.h). Lookup has no separate
                // "empty" sentinel distinct from that value, so querying
                // uint256{} can legitimately match an untouched slot even
                // though it was never Set(). This is a property of
                // CuckooCache itself (also why cuckoocache.cpp's own fuzz
                // target makes no such never-a-hit-before-insert assertion),
                // not something HeaderPoWCache can special-case away
                // without its own is-empty tracking.
                assert(!hit);
            }
            // If the key *was* Set(), a miss is a legitimate eviction --
            // nothing to assert either way.
            break;
        }
        case 2: {
            const size_t reset_megabytes = fuzzed_data_provider.ConsumeIntegralInRange<size_t>(0, 16);
            cache.Reset(reset_megabytes << 20);
            shadow_inserted.clear();
            break;
        }
        }
    }
}
