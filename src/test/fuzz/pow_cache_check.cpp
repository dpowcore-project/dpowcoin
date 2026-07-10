// Copyright (c) 2026-present The Dpowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint256.h>
#include <chainparams.h>
#include <consensus/params.h>
#include <pow.h>
#include <pow_cache.h>
#include <primitives/block.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <test/fuzz/util.h>
#include <uint256.h>
#include <util/check.h>
#include <util/chaintype.h>

#include <algorithm>
#include <cstdint>

// This target fuzzes CheckProofOfWorkCached() (pow_cache.h/pow_cache.cpp)
// end-to-end: a real CBlockHeader -> real GetHash() as the cache key ->
// real GetArgon2idPoWHash() -> real CheckProofOfWork() -> the
// process-global HeaderPoWCache (GetHeaderPoWCache()). This is the
// "separate, much-smaller-scale differential test" that pow_cache.cpp's
// own comment says CheckProofOfWorkCached() belongs in: pow_cache.cpp
// itself never constructs a header or touches Argon2id, and
// pow_argon2id.cpp never touches the cache. This target is the only place
// the two paths meet.
//
// [Dpowcoin] Ported from bitweb 30.x's src/test/fuzz/pow_cache_check.cpp
// unchanged -- every API this target touches (FUZZ_TARGET(.init=...),
// SelectParams()/ChainType::REGTEST, Assert(), arith_uint256/
// UintToArith256, CheckProofOfWork()/GetArgon2idPoWHash()) is identical in
// this tree.
//
// Deliberately expensive (real Argon2id per iteration, roughly
// 300-1500 hashes/sec on a slow CPU) and deliberately narrow in scope:
// there are only a handful of branches to reach at all (cache hit / cache
// miss+pass / cache miss+fail), so depth of search matters far less here
// than in a wide parser -- a modest corpus grown over a short time window
// is expected to saturate coverage quickly, unlike pow_cache's own target.
//
// Oracle: compare the cached wrapper against an uncached, direct recompute
// of the identical check (CheckProofOfWork(header.GetArgon2idPoWHash(),
// header.nBits, params)) on the same header. Per CheckProofOfWorkCached()'s
// documented contract -- "a miss always falls back to a full, honest
// recompute -- behavior on miss is identical to having no cache" -- the
// two must always agree, regardless of whether this exact header was
// already cached by an earlier iteration in this same run. A mismatch
// here means the cache is either hiding a real failure (unsafe) or
// fabricating a pass (critical consensus bug).
void initialize_pow_cache_check()
{
    // REGTEST
    SelectParams(ChainType::REGTEST);
}

FUZZ_TARGET(pow_cache_check, .init = initialize_pow_cache_check)
{
    FuzzedDataProvider fuzzed_data_provider(buffer.data(), buffer.size());
    const Consensus::Params& consensus_params{Params().GetConsensus()};

    CBlockHeader header;
    header.nVersion = fuzzed_data_provider.ConsumeIntegral<int32_t>();
    header.nTime    = fuzzed_data_provider.ConsumeIntegral<uint32_t>();
    header.nNonce   = fuzzed_data_provider.ConsumeIntegral<uint32_t>();

    // A uniformly random nBits/hash pairing almost never passes
    // CheckProofOfWork(), and a run that never passes never reaches the
    // cache-hit branch at all. Bias nBits towards powLimit (easiest
    // possible target) part of the time so both the pass and fail paths
    // -- and therefore the cache-hit path -- actually get exercised.
    const arith_uint256 pow_limit = UintToArith256(consensus_params.powLimit);
    if (fuzzed_data_provider.ConsumeBool()) {
        header.nBits = pow_limit.GetCompact();
    } else {
        header.nBits = fuzzed_data_provider.ConsumeIntegral<uint32_t>();
    }

    // Fill hashPrevBlock and hashMerkleRoot with arbitrary 32-byte values
    // (same pattern as pow_argon2id.cpp) so header.GetHash() -- the cache
    // key -- varies across iterations instead of colliding on zeroed
    // fields every time.
    const auto prev  = fuzzed_data_provider.ConsumeBytes<uint8_t>(32);
    const auto mroot = fuzzed_data_provider.ConsumeBytes<uint8_t>(32);
    if (prev.size()  == 32) std::copy(prev.begin(),  prev.end(),  header.hashPrevBlock.begin());
    if (mroot.size() == 32) std::copy(mroot.begin(), mroot.end(), header.hashMerkleRoot.begin());

    // Ground truth: uncached, direct recompute of the exact same check
    // CheckProofOfWorkCached() wraps.
    const bool direct_result = CheckProofOfWork(header.GetArgon2idPoWHash(), header.nBits, consensus_params);

    // Cached wrapper, called twice on the identical header. The first
    // call may be a cache hit (if this exact header's hash was already
    // Set() by an earlier iteration in this same process) or a miss;
    // whichever it is, both calls must still agree with direct_result --
    // a hit is only ever recorded for a header that previously passed,
    // and a miss always falls back to the full recompute.
    const bool cached_result_1 = CheckProofOfWorkCached(header, consensus_params);
    const bool cached_result_2 = CheckProofOfWorkCached(header, consensus_params);

    Assert(cached_result_1 == direct_result);
    Assert(cached_result_2 == direct_result);
}
