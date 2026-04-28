// Copyright (c) 2026 The Dpowcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POW_CACHE_H
#define BITCOIN_POW_CACHE_H

#include <uint256.h>

#include <cstddef>
#include <cstdint>

class CBlockHeader;

/**
 * Thread-safe LRU cache for Dual PoW hashes (Yespower + Argon2id).
 *
 * Rationale: GetArgon2idPoWHash() uses a 32 MiB scratchpad and costs
 * ~80–150 ms per header. During -reindex, deep reorgs and RPC re-checks the
 * exact same 80-byte header is hashed many times. Memoizing by GetHash()
 * (double-SHA256 of the 80-byte serialized header) is consensus-neutral:
 * the PoW functions are pure functions of those 80 bytes, so cached lookups
 * are bit-identical to recomputation.
 *
 * Failure sentinels (PoWHashFailureSentinel) are NEVER cached — an OOM may
 * be transient, the next call must re-attempt the heavy computation.
 *
 * See doc design: .lumen/docs/15-pow-cache-design.md
 */
namespace pow_cache {

//! Default LRU capacity (per algorithm). 10000 entries × ~88 B ≈ 0.85 MB heap.
constexpr size_t DEFAULT_CAPACITY = 10000;

//! Initialize both caches with the given capacity. capacity == 0 disables
//! the cache (lookups always recompute and never store).
void Init(size_t capacity);

//! Free all cached data and disable further insertions.
void Shutdown();

//! Compute or fetch the Yespower PoW hash for `header`.
uint256 GetYespower(const CBlockHeader& header);

//! Compute or fetch the Argon2id PoW hash for `header`.
uint256 GetArgon2id(const CBlockHeader& header);

struct Stats {
    uint64_t hits{0};
    uint64_t misses{0};
    uint64_t evictions{0};
    uint64_t size{0};
    uint64_t capacity{0};
};

//! Combined stats for the Yespower cache.
Stats GetYespowerStats();

//! Combined stats for the Argon2id cache.
Stats GetArgon2idStats();

} // namespace pow_cache

#endif // BITCOIN_POW_CACHE_H
