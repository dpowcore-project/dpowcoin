// Copyright (c) 2026-present The Dpowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow_cache.h>

#include <pow.h>
#include <primitives/block.h>

HeaderPoWCache& GetHeaderPoWCache()
{
    // Constructed once, lazily, on first use, and lives for the rest of
    // the process -- this object itself is never destroyed or replaced.
    // Initialization of function-local statics is thread-safe since
    // C++11, so no extra locking is needed here. Starts at
    // DEFAULT_HEADER_POW_CACHE_BYTES; InitHeaderPoWCache() can resize it
    // (via HeaderPoWCache::Reset(), see pow_cache.h) at any point after
    // this, any number of times.
    static HeaderPoWCache cache;
    return cache;
}

void InitHeaderPoWCache(size_t max_size_bytes)
{
    GetHeaderPoWCache().Reset(max_size_bytes);
}

bool CheckProofOfWorkCached(const CBlockHeader& header, const Consensus::Params& params)
{
    const uint256 hash{header.GetHash()};
    if (GetHeaderPoWCache().Get(hash)) {
        return true;
    }
    if (!CheckProofOfWork(header.GetArgon2idPoWHash(), header.nBits, params)) {
        return false;
    }
    GetHeaderPoWCache().Set(hash);
    return true;
}
