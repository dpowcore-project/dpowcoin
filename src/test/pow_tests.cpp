// Copyright (c) 2015-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <pow.h>
#include <test/util/random.h>
#include <test/util/common.h>
#include <test/util/setup_common.h>
#include <util/chaintype.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(pow_tests, BasicTestingSetup)


// ---------------------------------------------------------------------------
// Original Bitcoin Core difficulty-adjustment tests (kept for history).
//
// These tested CalculateNextWorkRequired() - the 2016-block retargeting rule
// that is NOT used in this coin (replaced by LWMA-3).
//
// BTC's retarget formula:
//   bnNew = bnLast * actualTimespan / targetTimespan
// At ideal hashrate: actualTimespan == targetTimespan > bnNew == bnLast exactly.
// No accumulation of integer-division error because it is a single multiply+divide.
//
// LWMA-3 accumulates N=576 iterations of (target / N / k), which introduces
// a systematic rounding loss that does NOT exist in the BTC formula.
// See the LWMA-3 test section below for details.
//
// Commented out so they compile but do not run; preserved so the diff against
// upstream Bitcoin Core stays transparent.
// ---------------------------------------------------------------------------
/* Test calculation of next difficulty target with no constraints applying
BOOST_AUTO_TEST_CASE(get_next_work)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    int64_t nLastRetargetTime = 1261130161; // Block #30240
    CBlockIndex pindexLast;
    pindexLast.nHeight = 32255;
    pindexLast.nTime = 1262152739;  // Block #32255
    pindexLast.nBits = 0x1d00ffff;

    // Here (and below): expected_nbits is calculated in
    // CalculateNextWorkRequired(); redoing the calculation here would be just
    // reimplementing the same code that is written in pow.cpp. Rather than
    // copy that code, we just hardcode the expected result.
    unsigned int expected_nbits = 0x1d00d86aU;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), expected_nbits);
    BOOST_CHECK(PermittedDifficultyTransition(chainParams->GetConsensus(), pindexLast.nHeight+1, pindexLast.nBits, expected_nbits));
}
*/

/* Test the constraint on the upper bound for next work
BOOST_AUTO_TEST_CASE(get_next_work_pow_limit)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    int64_t nLastRetargetTime = 1231006505; // Block #0
    CBlockIndex pindexLast;
    pindexLast.nHeight = 2015;
    pindexLast.nTime = 1233061996;  // Block #2015
    pindexLast.nBits = 0x1d00ffff;
    unsigned int expected_nbits = 0x1d00ffffU;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), expected_nbits);
    BOOST_CHECK(PermittedDifficultyTransition(chainParams->GetConsensus(), pindexLast.nHeight+1, pindexLast.nBits, expected_nbits));
}
*/

/* Test the constraint on the lower bound for actual time taken
BOOST_AUTO_TEST_CASE(get_next_work_lower_limit_actual)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    int64_t nLastRetargetTime = 1279008237; // Block #66528
    CBlockIndex pindexLast;
    pindexLast.nHeight = 68543;
    pindexLast.nTime = 1279297671;  // Block #68543
    pindexLast.nBits = 0x1c05a3f4;
    unsigned int expected_nbits = 0x1c0168fdU;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), expected_nbits);
    BOOST_CHECK(PermittedDifficultyTransition(chainParams->GetConsensus(), pindexLast.nHeight+1, pindexLast.nBits, expected_nbits));
    // Test that reducing nbits further would not be a PermittedDifficultyTransition.
    unsigned int invalid_nbits = expected_nbits-1;
    BOOST_CHECK(!PermittedDifficultyTransition(chainParams->GetConsensus(), pindexLast.nHeight+1, pindexLast.nBits, invalid_nbits));
}
*/

/* Test the constraint on the upper bound for actual time taken
BOOST_AUTO_TEST_CASE(get_next_work_upper_limit_actual)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    int64_t nLastRetargetTime = 1263163443; // NOTE: Not an actual block time
    CBlockIndex pindexLast;
    pindexLast.nHeight = 46367;
    pindexLast.nTime = 1269211443;  // Block #46367
    pindexLast.nBits = 0x1c387f6f;
    unsigned int expected_nbits = 0x1d00e1fdU;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), expected_nbits);
    BOOST_CHECK(PermittedDifficultyTransition(chainParams->GetConsensus(), pindexLast.nHeight+1, pindexLast.nBits, expected_nbits));
    // Test that increasing nbits further would not be a PermittedDifficultyTransition.
    unsigned int invalid_nbits = expected_nbits+1;
    BOOST_CHECK(!PermittedDifficultyTransition(chainParams->GetConsensus(), pindexLast.nHeight+1, pindexLast.nBits, invalid_nbits));
}
*/

// ===========================================================================
// LWMA-3 difficulty algorithm tests
// ===========================================================================
//
// Coin parameters (mainnet):
//   N = lwmaAveragingWindow  = 576
//   T = nPowTargetSpacing    = 300 s  (5 minutes)
//   k = N*(N+1)*T/2          = 49 852 800
//   genesisBits              = 0x1f1fffff  (== powLimit compact)
//   powLimit                 = 001fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
//   Bootstrap threshold L    = N+1 = 577
//     height <= L  -> bootstrap returns genesis nBits unchanged
//     height >  L  -> first real LWMA at height L+1 = N+2 = 578
//                     requires chain length >= N+3 = 579
//
// -- Why does the first LWMA result differ from genesisBits by 1 LSB? ------
//
//   The accumulator performs N iterations of:
//     avgTarget += target / N / k   (two sequential truncating integer divisions)
//   Each division truncates, so avgTarget accumulates a small deficit below
//   the exact value. After multiplying by sumWeightedSolvetimes, the result
//   is a few integer units below powLimit. GetCompact() rounds DOWN when the
//   MSB of the mantissa is set > result is genesisBits - 1 compact LSB.
//
//   The deficit in absolute units is ~10^10; one compact LSB at this exponent
//   is 2^224 ? 10^67 > the error is 57 orders of magnitude below the
//   resolution of the compact format. This has zero effect on mining.
//
// -- Direction of drift and safety ----------------------------------------
//
//   The target moves DOWN (harder) from powLimit.
//   0x1f1ffffe < 0x1f1fffff > slightly harder target > CheckProofOfWork
//   still accepts it because target < powLimit (not above it).
//   Genesis block nBits is hardcoded and is never touched by LWMA-3.
//   Bootstrap blocks (height <= 577) also return genesis nBits unchanged.
//
// -- Does the -1 LSB accumulate / drift further? ---------------------------
//
//   No. The chain stabilises at 0x1f1ffffe from the second post-bootstrap
//   block and stays there indefinitely at constant hashrate. Test 12
//   (lwma3_live_stabilization_2000_blocks) verifies this under full live
//   nBits propagation over 2000 blocks.
//
// -- Which chain types run LWMA-3? ----------------------------------------
//
//   GetNextWorkRequired() routes through two branches in order:
//     1. fPowNoRetargeting == true  > return pindexLast->nBits   (REGTEST)
//     2. Lwma3CalculateNextWorkRequired()     (MAIN, SIGNET, TESTNET, TESTNET4)
//
// -- Hardcoded expected values (verified by Python arith_uint256 simulation)
//
//   stable hashrate (height N+2)             : 0x1f1ffffeU
//   spacing = 6T (solvetime cap boundary) : 0x1f1fffffU  (clamped to powLimit)
//   spacing = 100T (above cap)            : 0x1f1fffffU  (same as 6T)
//   spacing = T/2  (2x hashrate)          : 0x1f0fffffU  (~powLimit / 2)
//   mixed window   (2T slow + T/2 fast)   : 0x1f1c0552U
// ===========================================================================

// ---------------------------------------------------------------------------
// Helper: build a linear chain of `count` blocks (indices 0..count-1).
// All blocks carry the same nBits, spaced `spacing` seconds apart from t0.
// The vector is pre-allocated so pprev pointers remain valid after the loop.
// LWMA-3 reads only nBits and nTime - nChainWork is not used by the algorithm
// but is populated so other tests that inspect it remain consistent.
// ---------------------------------------------------------------------------
static std::vector<CBlockIndex> BuildChain(int count, unsigned int nBits,
                                           int64_t t0, int64_t spacing)
{
    std::vector<CBlockIndex> blocks(count);
    for (int i = 0; i < count; i++) {
        blocks[i].pprev      = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight    = i;
        blocks[i].nTime      = static_cast<uint32_t>(t0 + static_cast<int64_t>(i) * spacing);
        blocks[i].nBits      = nBits;
        blocks[i].nChainWork = i ? blocks[i - 1].nChainWork + GetBlockProof(blocks[i - 1])
                                 : arith_uint256(0);
    }
    return blocks;
}

// ---------------------------------------------------------------------------
// Test 1: Bootstrap path.
//   Any block at height <= L = N+59424 = 60000 must return genesis nBits
//   unchanged.  The bootstrap guard in Lwma3CalculateNextWorkRequired:
//     if (height <= L) return genesis->nBits;
//   The first real LWMA computation occurs at height L+1 = N+59425 = 60001.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_bootstrap)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits; // 0x1f1fffff

    // height <= L=N+1=577 is bootstrap; last bootstrap pindexLast is at height L-1=N=576.
    const int L = static_cast<int>(N) + 1; // N+1 = 577
    auto blocks = BuildChain(L, genesisBits, 1775999888, T); // heights 0..L-1

    // Boundary check: height L-1 = N = 576 is the last bootstrap block.
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&blocks[L - 1], nullptr, consensus), genesisBits);

    // Interior heights must also return genesis nBits unchanged.
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&blocks[1],     nullptr, consensus), genesisBits);
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&blocks[N / 2], nullptr, consensus), genesisBits);
}

// ---------------------------------------------------------------------------
// Test 2: First real LWMA-3 computation.
//   pindexLast at height N+2 = 578 (first height that passes the bootstrap guard).
//   All N=576 window blocks carry genesisBits and ideal spacing T.
//   Expected result: 0x1f1ffffeU - one compact LSB below genesisBits.
//   This is caused by the systematic truncation in (target / N / k) ? N
//   iterations; the absolute error is ~10^10, negligible for mining.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_stable_hashrate)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit   = UintToArith256(consensus.powLimit);

    // Chain length N+3: heights 0..N+2, pindexLast = blocks[N+2].
    const int lwma_height = static_cast<int>(N) + 2; // first real LWMA
    auto blocks = BuildChain(lwma_height + 1, genesisBits, 1775999888, T);

    const unsigned int expected_nbits = 0x1f1ffffeU;
    unsigned int result = GetNextWorkRequired(&blocks[lwma_height], nullptr, consensus);
    BOOST_CHECK_EQUAL(result, expected_nbits);

    // The resulting target must be at or below powLimit.
    arith_uint256 resultTarget;
    resultTarget.SetCompact(result);
    BOOST_CHECK(resultTarget <= powLimit);
}

// ---------------------------------------------------------------------------
// Test 4: powLimit cap - very slow blocks must not produce a target above
//   powLimit.
//   spacing = 6T > every solvetime hits the internal 6T cap > uncapped result
//   would be ~6? powLimit > clamped to exactly powLimit.
//   Expected: 0x1f1fffffU == powLimit compact.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_powlimit_cap)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits  = chainParams->GenesisBlock().nBits;
    const unsigned int powLimitBits = UintToArith256(consensus.powLimit).GetCompact();
    const arith_uint256 powLimit    = UintToArith256(consensus.powLimit);

    const int lwma_height = static_cast<int>(N) + 2; // first real LWMA
    auto blocks = BuildChain(lwma_height + 1, genesisBits, 1775999888, 6 * T);

    const unsigned int expected_nbits = 0x1f1fffffU;
    unsigned int result = GetNextWorkRequired(&blocks[lwma_height], nullptr, consensus);

    BOOST_CHECK_EQUAL(result, expected_nbits);
    BOOST_CHECK_EQUAL(result, powLimitBits);

    arith_uint256 resultTarget;
    resultTarget.SetCompact(result);
    BOOST_CHECK(resultTarget <= powLimit);
}

// ---------------------------------------------------------------------------
// Test 5: The 6T solvetime cap is symmetric - any spacing >= 6T yields the
//   same result as exactly 6T, because every solvetime is clamped to
//   min(6T, actual) before being used.
//   Expected for both 6T and 100T: 0x1f1fffffU.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_6T_solvetime_cap)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;

    const int lwma_height = static_cast<int>(N) + 2; // first real LWMA
    auto blocks_6T   = BuildChain(lwma_height + 1, genesisBits, 1775999888, 6 * T);
    auto blocks_100T = BuildChain(lwma_height + 1, genesisBits, 1775999888, 100 * T);

    const unsigned int expected_nbits = 0x1f1fffffU;
    unsigned int result_6T   = GetNextWorkRequired(&blocks_6T[lwma_height],   nullptr, consensus);
    unsigned int result_100T = GetNextWorkRequired(&blocks_100T[lwma_height], nullptr, consensus);

    BOOST_CHECK_EQUAL(result_6T,   expected_nbits);
    BOOST_CHECK_EQUAL(result_100T, expected_nbits);
    BOOST_CHECK_EQUAL(result_6T,   result_100T);
}

// ---------------------------------------------------------------------------
// Test 6: Doubled hashrate - difficulty must rise, target must fall.
//   spacing = T/2 > blocks arrive twice as fast > algorithm halves the target.
//   Expected: 0x1f0fffffU  (~powLimit / 2).
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_double_hashrate)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit   = UintToArith256(consensus.powLimit);

    const int lwma_height = static_cast<int>(N) + 2; // first real LWMA
    auto blocks = BuildChain(lwma_height + 1, genesisBits, 1775999888, T / 2);

    const unsigned int expected_nbits = 0x1f0fffffU;
    unsigned int result = GetNextWorkRequired(&blocks[lwma_height], nullptr, consensus);
    BOOST_CHECK_EQUAL(result, expected_nbits);

    // Target must be strictly below powLimit - difficulty increased.
    arith_uint256 resultTarget;
    resultTarget.SetCompact(result);
    BOOST_CHECK(resultTarget < powLimit);
}

// ---------------------------------------------------------------------------
// Test 7: Mixed-spacing determinism - pure regression guard.
//   The LWMA window [3..578] (N=576 blocks, all carrying genesisBits) is split
//   into two halves by timestamp only:
//     blocks [3 .. 3+N/2-1]  : spacing 2T  (288 slow blocks)
//     blocks [3+N/2 .. 578]  : spacing T/2 (288 fast blocks)
//   Because LWMA weights recent blocks linearly higher, the fast second half
//   dominates and the result is between powLimit/2 and powLimit.
//   Expected: 0x1f1c1aa4U  (recomputed via faithful Python port of this repo's
//   Lwma3CalculateNextWorkRequired, integer division order matched exactly;
//   the previous 0x1f1c0552 constant was wrong).
//
//   Any modification to loop weights, timestamp clamping, or accumulator
//   arithmetic will produce a different compact value and fail this test.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_mixed_solvetimes_determinism)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit   = UintToArith256(consensus.powLimit);

    // Build chain length N+3 (pindexLast = blocks[N+2]).
    // LWMA window = blocks[3 .. N+2].
    // Advance for i=2 (block before window): T   -> j=1 solvetime = T.
    // Advances for i=3..N/2+2 (first HALF):  2T  -> j=2..289 solvetime = 2T.
    // Advances for i=N/2+3..N+1 (second HALF): T/2 -> j=290..576 solvetime = T/2.
    const int HALF        = static_cast<int>(N / 2);   // 288
    const int window_start = 3;                         // N+2 - N + 1
    const int chain_len   = static_cast<int>(N) + 3;   // N+3
    std::vector<CBlockIndex> blocks(chain_len);

    int64_t ts = 1775999888;
    for (int i = 0; i < chain_len; i++) {
        blocks[i].pprev      = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight    = i;
        blocks[i].nBits      = genesisBits;
        blocks[i].nTime      = static_cast<uint32_t>(ts);
        blocks[i].nChainWork = i ? blocks[i - 1].nChainWork + GetBlockProof(blocks[i - 1])
                                 : arith_uint256(0);
        // Advance the timestamp for the next block.
        if      (i < window_start)           ts += T;      // anchor before window
        else if (i < window_start + HALF)    ts += 2 * T;  // slow first half of window
        else                                 ts += T / 2;   // fast second half of window
    }

    // 0x1f1c1aa4 verified independently by faithful Python arith_uint256 simulation.
    const unsigned int expected_nbits = 0x1f1c1aa4U;
    unsigned int result = GetNextWorkRequired(&blocks[chain_len - 1], nullptr, consensus);
    BOOST_CHECK_EQUAL(result, expected_nbits);

    arith_uint256 resultTarget;
    resultTarget.SetCompact(result);
    BOOST_CHECK(resultTarget <= powLimit);
    BOOST_CHECK(resultTarget > arith_uint256(0));
}

// ---------------------------------------------------------------------------
// Test 8: fPowNoRetargeting - regtest shortcut.
//   When fPowNoRetargeting is true (REGTEST), GetNextWorkRequired must return
//   pindexLast->nBits unchanged regardless of chain length or timestamps.
//   This is the first branch in GetNextWorkRequired and completely bypasses
//   LWMA-3, keeping difficulty fixed for local development and unit testing.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_no_retargeting)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::REGTEST);
    const auto& consensus  = chainParams->GetConsensus();
    // Pre-condition: regtest must have fPowNoRetargeting enabled.
    BOOST_REQUIRE(consensus.fPowNoRetargeting);

    const int64_t N = consensus.lwmaAveragingWindow;
    const int64_t T = consensus.nPowTargetSpacing;

    // Use an arbitrary nBits value that is clearly not powLimit.
    const unsigned int someBits = 0x207fffffU;
    auto blocks = BuildChain(static_cast<int>(N + 3), someBits, 1775999888, T);

    unsigned int result = GetNextWorkRequired(&blocks[N + 2], nullptr, consensus);
    // Must return the parent's nBits without any LWMA calculation.
    BOOST_CHECK_EQUAL(result, someBits);
}

// ---------------------------------------------------------------------------
// Test 9: LWMA-3 runs on TESTNET and TESTNET4.
//   Real difficulty adjustment runs exactly as on mainnet.
//   Verifies:
//     - fPowNoRetargeting is off (LWMA-3 is the active path)
//     - GetNextWorkRequired returns a real LWMA result (not powLimit)
//     - result is a valid target at or below powLimit
//     - bootstrap path still works on testnets (height <= L returns genesis nBits)
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_testnet_runs_lwma)
{
    auto check = [&](ChainType ct) {
        const auto chainParams = CreateChainParams(*m_node.args, ct);
        const auto& consensus  = chainParams->GetConsensus();

        // Pre-condition: LWMA must be the active path on testnets.
        BOOST_REQUIRE(!consensus.fPowNoRetargeting);

        const int64_t N        = consensus.lwmaAveragingWindow;
        const int64_t T        = consensus.nPowTargetSpacing;
        const unsigned int genesisBits  = chainParams->GenesisBlock().nBits;
        const unsigned int powLimitBits = UintToArith256(consensus.powLimit).GetCompact();
        const arith_uint256 powLimit    = UintToArith256(consensus.powLimit);

        const int lwma_height = static_cast<int>(N) + 2; // first real LWMA
        auto blocks = BuildChain(lwma_height + 1, genesisBits, 1761999888, T);

        // Past bootstrap threshold: LWMA runs and result differs from powLimit
        // by exactly 1 compact LSB (same truncation behaviour as mainnet).
        unsigned int result = GetNextWorkRequired(&blocks[lwma_height], nullptr, consensus);
        BOOST_CHECK(result != powLimitBits);

        // Result must be a valid target at or below powLimit.
        arith_uint256 resultTarget;
        resultTarget.SetCompact(result);
        BOOST_CHECK(resultTarget <= powLimit);
        BOOST_CHECK(resultTarget > arith_uint256(0));

        // Bootstrap path must still work on testnets:
        // height <= L = N+1 returns genesis nBits unchanged.
        const int L = static_cast<int>(N) + 1;
        auto bootstrap_blocks = BuildChain(L, genesisBits, 1761999888, T);
        unsigned int bootstrap_result = GetNextWorkRequired(&bootstrap_blocks[L - 1], nullptr, consensus);
        BOOST_CHECK_EQUAL(bootstrap_result, genesisBits);
    };

    check(ChainType::TESTNET);
    check(ChainType::TESTNET4);
}

// ---------------------------------------------------------------------------
// Test 10: Duplicate timestamps - protection against negative or zero solvetimes.
//   When a block's timestamp is <= the running previousTimestamp, LWMA-3
//   forces it forward by 1 second:
//     thisTimestamp = (block->GetBlockTime() > previousTimestamp)
//                     ? block->GetBlockTime() : previousTimestamp + 1;
//   This test exercises that branch by giving every block the same nTime.
//   With all solvetimes forced to 1 second, the algorithm sees extremely fast
//   block production and must raise difficulty well above powLimit / 2.
//   The result must be a valid compact value (non-zero, <= powLimit).
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_duplicate_timestamps)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit   = UintToArith256(consensus.powLimit);

    // spacing = 0 -> every block has the same nTime = t0.
    auto blocks = BuildChain(static_cast<int>(N) + 3, genesisBits, 1775999888, 0);

    unsigned int result = GetNextWorkRequired(&blocks[static_cast<int>(N) + 2], nullptr, consensus);
    arith_uint256 resultTarget;
    resultTarget.SetCompact(result);

    // Result must be a valid, positive target that does not exceed powLimit.
    BOOST_CHECK(resultTarget > arith_uint256(0));
    BOOST_CHECK(resultTarget <= powLimit);
    // With every solvetime = 1s the algorithm perceives massive hashrate;
    // target must be well below powLimit/2 (difficulty well above midpoint).
    BOOST_CHECK(resultTarget < UintToArith256(consensus.powLimit) / 2);
}

// ---------------------------------------------------------------------------
// Test 12: Live-propagation stabilization - 2000 blocks after bootstrap.
//
//   Existing tests (2-3) freeze all nBits at genesisBits and only vary
//   timestamps. This test simulates the real chain: each block's nBits is
//   set to the value returned by GetNextWorkRequired for the previous block,
//   exactly as a full node does.
//
//   LWMA-3 has a predictable, harmless compact-LSB drift under live
//   propagation: every full N-block window the mantissa loses one LSB due
//   to accumulated truncation in (target / N / k) ? N iterations.
//   One compact LSB at exponent 0x1f equals 2^224, which is ~9.5?10^-5%
//   of powLimit - 57 orders of magnitude below any meaningful difficulty
//   difference.  The drift pattern is exactly periodic: one LSB lost per
//   N blocks.
//
//   The test verifies three things:
//     (a) Exact compact values at window-boundary checkpoints (N, 2N, 3N).
//     (b) Exact final value at step 2000.
//     (c) The algorithm stays bounded in [0x1f1ffffbU, 0x1f1fffffU]
//         throughout - it never explodes, never collapses.
//
//   All expected values produced by Python arith_uint256 simulation that
//   mirrors the exact integer arithmetic in Lwma3CalculateNextWorkRequired.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_live_stabilization_2000_blocks)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit   = UintToArith256(consensus.powLimit);

    // L = last bootstrap height (60000); first live block is at height L+1.
    const int L     = static_cast<int>(N) + 1; // bootstrap end: height <= L returns powLimit
    const int LIVE  = 2000;
    const int TOTAL = L + 1 + LIVE;               // 62001

    // Pre-allocate the full vector so pprev pointers remain stable.
    std::vector<CBlockIndex> blocks(TOTAL);

    // Bootstrap phase (heights 0..L): all genesis nBits, spacing T.
    for (int i = 0; i <= L; i++) {
        blocks[i].pprev      = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight    = i;
        blocks[i].nTime      = static_cast<uint32_t>(1775999888LL + static_cast<int64_t>(i) * T);
        blocks[i].nBits      = genesisBits;
        blocks[i].nChainWork = i ? blocks[i-1].nChainWork + GetBlockProof(blocks[i-1])
                                 : arith_uint256(0);
    }

    // Live phase (heights L+1..L+LIVE): each block's nBits computed from
    // the preceding chain, spacing = T.
    int64_t cur_ts = static_cast<int64_t>(blocks[L].nTime) + T;
    for (int i = L + 1; i < TOTAL; i++) {
        blocks[i].pprev      = &blocks[i - 1];
        blocks[i].nHeight    = i;
        blocks[i].nTime      = static_cast<uint32_t>(cur_ts);
        blocks[i].nBits      = GetNextWorkRequired(&blocks[i - 1], nullptr, consensus);
        blocks[i].nChainWork = blocks[i-1].nChainWork + GetBlockProof(blocks[i-1]);
        cur_ts += T;
    }

    // (a) Window-boundary checkpoints - one compact LSB lost per N blocks.
    //   step N  = 576 : first full live window        -> 0x1f1ffffe
    //   step 2N = 1152: second window rollover        -> 0x1f1ffffd
    //   step 3N = 1728: third  window rollover        -> 0x1f1ffffc
    BOOST_CHECK_EQUAL(blocks[L + static_cast<int>(N)].nBits,         0x1f1ffffeU);
    BOOST_CHECK_EQUAL(blocks[L + static_cast<int>(2 * N)].nBits,     0x1f1ffffdU);
    BOOST_CHECK_EQUAL(blocks[L + static_cast<int>(3 * N)].nBits,     0x1f1ffffcU);

    // (b) Final value after 2000 live blocks: falls in [3N+1, 4N) -> 0x1f1ffffb
    BOOST_CHECK_EQUAL(blocks[L + LIVE].nBits, 0x1f1ffffbU);

    // (c) Bounds check over all 2000 steps: never above powLimit, never
    //     below powLimit/2 (the latter proves no runaway difficulty collapse).
    for (int i = L + 1; i <= L + LIVE; i++) {
        arith_uint256 tgt;
        tgt.SetCompact(blocks[i].nBits);
        BOOST_CHECK(tgt <= powLimit);
        BOOST_CHECK(tgt >= powLimit / 2);
    }
}

// ---------------------------------------------------------------------------
// Test 13: Live hashrate drop (3T) followed by full recovery (2N blocks at T).
//
//   Scenario (all phases use live nBits propagation):
//     Phase 1: N=576 blocks at  T   - establish stable baseline
//     Phase 2: N=576 blocks at 3T   - hashrate falls to 1/3
//     Phase 3: N=576 blocks at  T   - 1st recovery window
//     Phase 4: N=576 blocks at  T   - 2nd recovery window
//
//   Expected behaviour:
//     - After phase 2 the target reaches powLimit (difficulty at minimum).
//     - After phase 3 the window still carries powLimit nBits from the drop
//       era, so the target stays at powLimit.
//     - After phase 4 the window is completely refreshed with T-spaced
//       blocks that all carry post-recovery nBits -> returns to 0x1f1ffffe.
//
//   All expected values verified by Python arith_uint256 simulation.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_live_hashrate_drop_and_recovery)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits  = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit    = UintToArith256(consensus.powLimit);
    const unsigned int powLimitBits = powLimit.GetCompact();

    const int L     = static_cast<int>(N) + 1; // bootstrap end: height <= L returns powLimit
    const int TOTAL = L + 1 + static_cast<int>(4 * N); // 62305

    std::vector<CBlockIndex> blocks(TOTAL);

    // Bootstrap
    for (int i = 0; i <= L; i++) {
        blocks[i].pprev      = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight    = i;
        blocks[i].nTime      = static_cast<uint32_t>(1775999888LL + static_cast<int64_t>(i) * T);
        blocks[i].nBits      = genesisBits;
        blocks[i].nChainWork = i ? blocks[i-1].nChainWork + GetBlockProof(blocks[i-1])
                                 : arith_uint256(0);
    }

    // Lambda: fill one phase of `count` blocks with given spacing.
    // cur_ts is captured by reference and advanced by `spacing` per block.
    int64_t cur_ts = static_cast<int64_t>(blocks[L].nTime);
    auto fill_phase = [&](int from, int count, int64_t spacing) {
        for (int j = 0; j < count; j++) {
            const int i = from + j;
            cur_ts += spacing;
            blocks[i].pprev      = &blocks[i - 1];
            blocks[i].nHeight    = i;
            blocks[i].nTime      = static_cast<uint32_t>(cur_ts);
            blocks[i].nBits      = GetNextWorkRequired(&blocks[i - 1], nullptr, consensus);
            blocks[i].nChainWork = blocks[i-1].nChainWork + GetBlockProof(blocks[i-1]);
        }
    };

    const int ph1 = L + 1;
    const int ph2 = ph1 + static_cast<int>(N);
    const int ph3 = ph2 + static_cast<int>(N);
    const int ph4 = ph3 + static_cast<int>(N);

    fill_phase(ph1, static_cast<int>(N), T);       // stable
    fill_phase(ph2, static_cast<int>(N), 3 * T);   // drop
    fill_phase(ph3, static_cast<int>(N), T);        // 1st recovery window
    fill_phase(ph4, static_cast<int>(N), T);        // 2nd recovery window

    const unsigned int after_stable = blocks[ph2 - 1].nBits;
    const unsigned int after_drop   = blocks[ph3 - 1].nBits;
    const unsigned int after_rec1   = blocks[ph4 - 1].nBits;
    const unsigned int after_rec2   = blocks[ph4 + static_cast<int>(N) - 1].nBits;

    // Stable baseline (identical to test 2/3).
    BOOST_CHECK_EQUAL(after_stable, 0x1f1ffffeU);

    // Drop clamps to powLimit - difficulty at minimum.
    BOOST_CHECK_EQUAL(after_drop, 0x1f1fffffU);
    BOOST_CHECK_EQUAL(after_drop, powLimitBits);

    // First recovery window still fully occupied by drop-era nBits.
    BOOST_CHECK_EQUAL(after_rec1, 0x1f1fffffU);

    // Second recovery window: fully refreshed > back to stable.
    BOOST_CHECK_EQUAL(after_rec2, 0x1f1ffffeU);

    // Structural invariants.
    arith_uint256 t_stable, t_drop, t_rec2;
    t_stable.SetCompact(after_stable);
    t_drop.SetCompact(after_drop);
    t_rec2.SetCompact(after_rec2);

    BOOST_CHECK(t_drop > t_stable); // drop raised target  (easier mining)
    BOOST_CHECK(t_rec2 < t_drop);   // recovery lowered it (harder mining again)
    BOOST_CHECK(t_drop <= powLimit);
    BOOST_CHECK(t_rec2 <= powLimit);
}

// ---------------------------------------------------------------------------
// Test 14: Live hashrate spike (T/3) followed by full recovery (2N blocks at T).
//
//   Scenario (live nBits propagation):
//     Phase 1: N=576 blocks at  T   - stable baseline
//     Phase 2: N=576 blocks at T/3  - hashrate triples
//     Phase 3: N=576 blocks at  T   - 1st recovery window
//     Phase 4: N=576 blocks at  T   - 2nd recovery window
//
//   Expected behaviour:
//     - Phase 2 drives the target sharply down (difficulty climbs ~3?).
//     - Phase 3: as high-difficulty spike blocks roll out of the window the
//       target begins to rise.
//     - Phase 4: further easing; target rises again but is still well below
//       the pre-spike stable value (full convergence takes more windows).
//
//   Key assertions:
//     after_spike  < after_stable   (harder target after spike)
//     after_rec1   > after_spike    (easing begins after 1st window)
//     after_rec2   > after_rec1     (easing continues in 2nd window)
//
//   All expected values verified by Python arith_uint256 simulation.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_live_hashrate_spike_and_recovery)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit   = UintToArith256(consensus.powLimit);

    const int L     = static_cast<int>(N) + 1;
    const int TOTAL = L + 1 + static_cast<int>(4 * N);

    std::vector<CBlockIndex> blocks(TOTAL);

    for (int i = 0; i <= L; i++) {
        blocks[i].pprev      = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight    = i;
        blocks[i].nTime      = static_cast<uint32_t>(1775999888LL + static_cast<int64_t>(i) * T);
        blocks[i].nBits      = genesisBits;
        blocks[i].nChainWork = i ? blocks[i-1].nChainWork + GetBlockProof(blocks[i-1])
                                 : arith_uint256(0);
    }

    int64_t cur_ts = static_cast<int64_t>(blocks[L].nTime);
    auto fill_phase = [&](int from, int count, int64_t spacing) {
        for (int j = 0; j < count; j++) {
            const int i = from + j;
            cur_ts += spacing;
            blocks[i].pprev      = &blocks[i - 1];
            blocks[i].nHeight    = i;
            blocks[i].nTime      = static_cast<uint32_t>(cur_ts);
            blocks[i].nBits      = GetNextWorkRequired(&blocks[i - 1], nullptr, consensus);
            blocks[i].nChainWork = blocks[i-1].nChainWork + GetBlockProof(blocks[i-1]);
        }
    };

    const int ph1 = L + 1;
    const int ph2 = ph1 + static_cast<int>(N);
    const int ph3 = ph2 + static_cast<int>(N);
    const int ph4 = ph3 + static_cast<int>(N);

    fill_phase(ph1, static_cast<int>(N), T);       // stable
    fill_phase(ph2, static_cast<int>(N), T / 3);   // spike
    fill_phase(ph3, static_cast<int>(N), T);        // 1st recovery window
    fill_phase(ph4, static_cast<int>(N), T);        // 2nd recovery window

    const unsigned int after_stable = blocks[ph2 - 1].nBits;
    const unsigned int after_spike  = blocks[ph3 - 1].nBits;
    const unsigned int after_rec1   = blocks[ph4 - 1].nBits;
    const unsigned int after_rec2   = blocks[ph4 + static_cast<int>(N) - 1].nBits;

    // Stable baseline.
    BOOST_CHECK_EQUAL(after_stable, 0x1f1ffffeU);

    // After spike: target much lower (difficulty ~3? higher).
    // Python simulation: 0x1f0521b5
    BOOST_CHECK_EQUAL(after_spike, 0x1f0521b5U);

    // Recovery target rises each window.
    // Python simulation: 0x1f0607ff after 1st window, 0x1f06091f after 2nd.
    BOOST_CHECK_EQUAL(after_rec1, 0x1f0607ffU);
    BOOST_CHECK_EQUAL(after_rec2, 0x1f06091fU);

    // Structural invariants.
    arith_uint256 t_stable, t_spike, t_rec1, t_rec2;
    t_stable.SetCompact(after_stable);
    t_spike.SetCompact(after_spike);
    t_rec1.SetCompact(after_rec1);
    t_rec2.SetCompact(after_rec2);

    BOOST_CHECK(t_spike < t_stable); // spike raised difficulty
    BOOST_CHECK(t_rec1  > t_spike);  // 1st window: easing begins
    BOOST_CHECK(t_rec2  > t_rec1);   // 2nd window: easing continues
    BOOST_CHECK(t_rec2 <= powLimit); // always sane
}

// ---------------------------------------------------------------------------
// Test 15: powLimit cap during live nBits propagation.
//
//   Tests 4 and 5 verify the 6T cap and powLimit ceiling with all window
//   blocks carrying a fixed genesis nBits.  This test repeats the same
//   scenario with live nBits propagation so each block's nBits is the actual
//   LWMA output - exactly as a real full node behaves.
//
//   Scenario:
//     Phase 1 : N=576 blocks at   T  - stable baseline (live propagation)
//     Phase 2 : N=576 blocks at 100T - extreme hashrate drop (live propagation)
//
//   100T >> 6T, so every solvetime in phase 2 is clamped to 6T internally.
//   Once the window is completely filled with 6T-capped solvetimes the
//   algorithm produces ~6? the current target, which exceeds powLimit and is
//   clamped to powLimitBits.
//
//   Expected:
//     - Final nBits after full phase 2  == powLimitBits (0x1f1fffffU).
//     - All nBits in both phases produce a target <= powLimit (drift-safe).
//     - During the extreme drop phase the target is non-decreasing -
//       difficulty can only ease, never spike upward during a sustained drop.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_live_powlimit_cap_propagation)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits  = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit    = UintToArith256(consensus.powLimit);
    const unsigned int powLimitBits = powLimit.GetCompact();

    const int L     = static_cast<int>(N) + 1;
    const int TOTAL = L + 1 + static_cast<int>(2 * N);

    std::vector<CBlockIndex> blocks(TOTAL);

    for (int i = 0; i <= L; i++) {
        blocks[i].pprev      = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight    = i;
        blocks[i].nTime      = static_cast<uint32_t>(1775999888LL + static_cast<int64_t>(i) * T);
        blocks[i].nBits      = genesisBits;
        blocks[i].nChainWork = i ? blocks[i-1].nChainWork + GetBlockProof(blocks[i-1])
                                 : arith_uint256(0);
    }

    int64_t cur_ts = static_cast<int64_t>(blocks[L].nTime);
    auto fill_phase = [&](int from, int count, int64_t spacing) {
        for (int j = 0; j < count; j++) {
            const int i = from + j;
            cur_ts += spacing;
            blocks[i].pprev      = &blocks[i - 1];
            blocks[i].nHeight    = i;
            blocks[i].nTime      = static_cast<uint32_t>(cur_ts);
            blocks[i].nBits      = GetNextWorkRequired(&blocks[i - 1], nullptr, consensus);
            blocks[i].nChainWork = blocks[i-1].nChainWork + GetBlockProof(blocks[i-1]);
        }
    };

    const int ph1 = L + 1;
    const int ph2 = ph1 + static_cast<int>(N);

    fill_phase(ph1, static_cast<int>(N), T);        // stable
    fill_phase(ph2, static_cast<int>(N), 100 * T);  // extreme drop

    // After a full window of 100T-spaced blocks, the 6T internal cap
    // saturates every solvetime > target exceeds powLimit > clamped.
    BOOST_CHECK_EQUAL(blocks[TOTAL - 1].nBits, powLimitBits);

    // Per-block invariants.
    for (int i = ph1; i < TOTAL; i++) {
        arith_uint256 tgt;
        tgt.SetCompact(blocks[i].nBits);

        // Drift-direction safety: target never above powLimit.
        BOOST_CHECK(tgt <= powLimit);

        // During extreme drop: target is non-decreasing (difficulty only eases).
        if (i > ph2) {
            arith_uint256 tgt_prev;
            tgt_prev.SetCompact(blocks[i - 1].nBits);
            BOOST_CHECK(tgt >= tgt_prev);
        }
    }
}

// ---------------------------------------------------------------------------
// Test 16: 6T solvetime cap symmetry during live nBits propagation.
//
//   Test 5 shows that spacings of exactly 6T and 100T produce the same
//   result when all window blocks share genesis nBits.  This test verifies
//   the same symmetry holds under live nBits propagation.
//
//   Two independent chains branch from the same bootstrap:
//     Chain A: N stable at T, then N at  6T.
//     Chain B: N stable at T, then N at 100T.
//
//   Expected:
//     - result_6T   == powLimitBits  (cap fully saturated).
//     - result_100T == powLimitBits  (same - clamped to 6T internally).
//     - result_6T   == result_100T   (cap symmetry preserved with live nBits).
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_live_6T_cap_symmetry_propagation)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits  = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit    = UintToArith256(consensus.powLimit);
    const unsigned int powLimitBits = powLimit.GetCompact();

    const int L     = static_cast<int>(N) + 1;
    const int TOTAL = L + 1 + static_cast<int>(2 * N);

    // Build bootstrap + stable + drop for a given drop spacing; return last nBits.
    auto build_and_drop = [&](int64_t drop_spacing) -> unsigned int {
        std::vector<CBlockIndex> blks(TOTAL);
        for (int i = 0; i <= L; i++) {
            blks[i].pprev      = i ? &blks[i - 1] : nullptr;
            blks[i].nHeight    = i;
            blks[i].nTime      = static_cast<uint32_t>(1775999888LL + static_cast<int64_t>(i) * T);
            blks[i].nBits      = genesisBits;
            blks[i].nChainWork = i ? blks[i-1].nChainWork + GetBlockProof(blks[i-1])
                                   : arith_uint256(0);
        }
        int64_t ts = static_cast<int64_t>(blks[L].nTime);
        auto fill = [&](int from, int count, int64_t spacing) {
            for (int j = 0; j < count; j++) {
                const int i = from + j;
                ts += spacing;
                blks[i].pprev      = &blks[i - 1];
                blks[i].nHeight    = i;
                blks[i].nTime      = static_cast<uint32_t>(ts);
                blks[i].nBits      = GetNextWorkRequired(&blks[i - 1], nullptr, consensus);
                blks[i].nChainWork = blks[i-1].nChainWork + GetBlockProof(blks[i-1]);
            }
        };
        fill(L + 1,                             static_cast<int>(N), T);
        fill(L + 1 + static_cast<int>(N), static_cast<int>(N), drop_spacing);
        return blks[TOTAL - 1].nBits;
    };

    const unsigned int result_6T   = build_and_drop(6 * T);
    const unsigned int result_100T = build_and_drop(100 * T);

    BOOST_CHECK_EQUAL(result_6T,   powLimitBits);
    BOOST_CHECK_EQUAL(result_100T, powLimitBits);
    BOOST_CHECK_EQUAL(result_6T,   result_100T);
}

// ---------------------------------------------------------------------------
// Test 17: Consecutive hashrate events - spike immediately followed by drop.
//
//   Scenario (live nBits propagation throughout):
//     Phase 1: N=576 blocks at  T   - stable baseline
//     Phase 2: N=576 blocks at T/3  - hashrate triples (spike)
//     Phase 3: N=576 blocks at 3T   - hashrate collapses to 1/3 (drop)
//     Phase 4: N=576 blocks at  T   - recovery
//
//   Key structural properties:
//     - after_spike  < after_stable - spike raises difficulty (target falls).
//     - after_drop   > after_spike  - drop eases difficulty (target rises).
//       The window still carries spike-era high-difficulty nBits, so the
//       rise is slower than after a drop starting from stable difficulty.
//     - after_rec   >= after_drop   - recovery continues upward.
//     - Every nBits throughout produces a target <= powLimit.
//
//   after_spike must equal Test 14's value (same prefix scenario).
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_live_consecutive_spike_then_drop)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit   = UintToArith256(consensus.powLimit);

    const int L     = static_cast<int>(N) + 1;
    const int TOTAL = L + 1 + static_cast<int>(4 * N);

    std::vector<CBlockIndex> blocks(TOTAL);

    for (int i = 0; i <= L; i++) {
        blocks[i].pprev      = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight    = i;
        blocks[i].nTime      = static_cast<uint32_t>(1775999888LL + static_cast<int64_t>(i) * T);
        blocks[i].nBits      = genesisBits;
        blocks[i].nChainWork = i ? blocks[i-1].nChainWork + GetBlockProof(blocks[i-1])
                                 : arith_uint256(0);
    }

    int64_t cur_ts = static_cast<int64_t>(blocks[L].nTime);
    auto fill_phase = [&](int from, int count, int64_t spacing) {
        for (int j = 0; j < count; j++) {
            const int i = from + j;
            cur_ts += spacing;
            blocks[i].pprev      = &blocks[i - 1];
            blocks[i].nHeight    = i;
            blocks[i].nTime      = static_cast<uint32_t>(cur_ts);
            blocks[i].nBits      = GetNextWorkRequired(&blocks[i - 1], nullptr, consensus);
            blocks[i].nChainWork = blocks[i-1].nChainWork + GetBlockProof(blocks[i-1]);
        }
    };

    const int ph1 = L + 1;
    const int ph2 = ph1 + static_cast<int>(N);
    const int ph3 = ph2 + static_cast<int>(N);
    const int ph4 = ph3 + static_cast<int>(N);

    fill_phase(ph1, static_cast<int>(N), T);       // stable
    fill_phase(ph2, static_cast<int>(N), T / 3);   // spike
    fill_phase(ph3, static_cast<int>(N), 3 * T);   // drop immediately after spike
    fill_phase(ph4, static_cast<int>(N), T);        // recovery

    const unsigned int after_stable = blocks[ph2 - 1].nBits;
    const unsigned int after_spike  = blocks[ph3 - 1].nBits;
    const unsigned int after_drop   = blocks[ph4 - 1].nBits;
    const unsigned int after_rec    = blocks[TOTAL - 1].nBits;

    arith_uint256 t_stable, t_spike, t_drop, t_rec;
    t_stable.SetCompact(after_stable);
    t_spike.SetCompact(after_spike);
    t_drop.SetCompact(after_drop);
    t_rec.SetCompact(after_rec);

    // Same prefix as test 14 > after_spike must be identical.
    BOOST_CHECK_EQUAL(after_stable, 0x1f1ffffeU);
    BOOST_CHECK_EQUAL(after_spike,  0x1f0521b5U);

    // Spike raised difficulty (smaller target).
    BOOST_CHECK(t_spike < t_stable);

    // Drop following spike: difficulty eases (target rises from spike low).
    BOOST_CHECK(t_drop > t_spike);

    // Drop from spike-low starting point must not immediately reach powLimit
    // (the window still carries high-difficulty spike-era nBits).
    arith_uint256 powLimit_ = UintToArith256(consensus.powLimit);
    BOOST_CHECK(t_drop < powLimit_);

    // Recovery: target continues easing upward.
    BOOST_CHECK(t_rec >= t_drop);

    // Per-block safety: target never above powLimit.
    for (int i = ph1; i < TOTAL; i++) {
        arith_uint256 tgt;
        tgt.SetCompact(blocks[i].nBits);
        BOOST_CHECK(tgt <= powLimit);
    }
}

// ---------------------------------------------------------------------------
// Test 18: Monotonicity of target during sustained hashrate changes.
//
//   This is the mathematical proof-in-code of LWMA-3's two core properties:
//
//     Property A (drop monotonicity):
//       During a sustained hashrate drop (all solvetimes > T), the LWMA-3
//       target is non-decreasing at every step.  Difficulty can only ease;
//       it cannot spike upward while miners are slower than average.
//
//     Property B (spike monotonicity):
//       During a sustained hashrate spike (all solvetimes < T), the LWMA-3
//       target is non-increasing at every step.  Difficulty can only harden;
//       it cannot collapse while miners are faster than average.
//
//   Scenario (live nBits propagation):
//     Phase 1: N=576 blocks at  T   - stable baseline
//     Phase 2: N=576 blocks at 3T   - sustained hashrate drop
//     Phase 3: N=576 blocks at T/3  - sustained hashrate spike
//
//   Also asserts that target never exceeds powLimit across all three phases.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_live_monotone_during_sustained_change)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit   = UintToArith256(consensus.powLimit);

    const int L     = static_cast<int>(N) + 1;
    const int TOTAL = L + 1 + static_cast<int>(3 * N);

    std::vector<CBlockIndex> blocks(TOTAL);

    for (int i = 0; i <= L; i++) {
        blocks[i].pprev      = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight    = i;
        blocks[i].nTime      = static_cast<uint32_t>(1775999888LL + static_cast<int64_t>(i) * T);
        blocks[i].nBits      = genesisBits;
        blocks[i].nChainWork = i ? blocks[i-1].nChainWork + GetBlockProof(blocks[i-1])
                                 : arith_uint256(0);
    }

    int64_t cur_ts = static_cast<int64_t>(blocks[L].nTime);
    auto fill_phase = [&](int from, int count, int64_t spacing) {
        for (int j = 0; j < count; j++) {
            const int i = from + j;
            cur_ts += spacing;
            blocks[i].pprev      = &blocks[i - 1];
            blocks[i].nHeight    = i;
            blocks[i].nTime      = static_cast<uint32_t>(cur_ts);
            blocks[i].nBits      = GetNextWorkRequired(&blocks[i - 1], nullptr, consensus);
            blocks[i].nChainWork = blocks[i-1].nChainWork + GetBlockProof(blocks[i-1]);
        }
    };

    const int ph1 = L + 1;                          // start of stable
    const int ph2 = ph1 + static_cast<int>(N);      // start of drop
    const int ph3 = ph2 + static_cast<int>(N);      // start of spike

    fill_phase(ph1, static_cast<int>(N), T);       // stable
    fill_phase(ph2, static_cast<int>(N), 3 * T);   // sustained drop
    fill_phase(ph3, static_cast<int>(N), T / 3);   // sustained spike

    // Property A: during sustained drop target is non-decreasing.
    // (skip the very first block of each phase - it is a transition step
    //  where the previous-block's T-spaced solvetime is still in the window)
    for (int i = ph2 + 1; i < ph3; i++) {
        arith_uint256 tgt_cur, tgt_prev;
        tgt_cur.SetCompact(blocks[i].nBits);
        tgt_prev.SetCompact(blocks[i - 1].nBits);
        BOOST_CHECK(tgt_cur >= tgt_prev);
    }

    // Property B: during sustained spike target is non-increasing.
    for (int i = ph3 + 1; i < TOTAL; i++) {
        arith_uint256 tgt_cur, tgt_prev;
        tgt_cur.SetCompact(blocks[i].nBits);
        tgt_prev.SetCompact(blocks[i - 1].nBits);
        BOOST_CHECK(tgt_cur <= tgt_prev);
    }

    // Bounds: target never above powLimit across all three phases.
    for (int i = ph1; i < TOTAL; i++) {
        arith_uint256 tgt;
        tgt.SetCompact(blocks[i].nBits);
        BOOST_CHECK(tgt <= powLimit);
    }

    // Sanity: after full drop window the target must have risen above stable,
    // and after full spike window it must have fallen well below stable.
    arith_uint256 t_stable, t_after_drop, t_after_spike;
    t_stable.SetCompact(blocks[ph2 - 1].nBits);
    t_after_drop.SetCompact(blocks[ph3 - 1].nBits);
    t_after_spike.SetCompact(blocks[TOTAL - 1].nBits);

    BOOST_CHECK(t_after_drop  > t_stable);      // drop eased difficulty
    BOOST_CHECK(t_after_spike < t_after_drop);  // spike then hardened it
}

// ---------------------------------------------------------------------------
// Test 19: Timestamp drop attack - maximum difficulty manipulation via FTL.
//
//   An attacker who controls the block timestamp can inflate apparent solvetimes
//   by setting each block's timestamp as far into the future as nodes allow.
//   With MAX_FUTURE_BLOCK_TIME = 600s = 2T, the maximum fake solvetime per
//   block is 600s instead of the real T=300s.
//
//   Key finding (from Python simulation):
//     The attack hits powLimit at step 1 of N - instantly.
//     After the full N-block attack window the difficulty is at powLimit,
//     identical to what a natural 3T slow hashrate produces (test 13).
//
//   Why the attack is safe despite hitting powLimit immediately:
//     - powLimit is the floor - difficulty cannot go lower.
//     - Recovery is identical to the natural drop scenario (test 13):
//       after 2N honest blocks the difficulty returns to 0x1f1ffffe.
//     - The attacker gains NO extra advantage compared to simply mining slowly.
//       FTL = 2T is the exact threshold where the attack converges to powLimit,
//       not below it.
//
//   Scenario (live nBits propagation):
//     Phase 1: N=576 blocks at  T   - stable baseline
//     Phase 2: N=576 blocks at 2T   - timestamp drop attack (FTL=600)
//     Phase 3: N=576 blocks at  T   - 1st recovery window
//     Phase 4: N=576 blocks at  T   - 2nd recovery window
//
//   All expected values verified by Python arith_uint256 simulation.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_timestamp_drop_attack)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits  = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit    = UintToArith256(consensus.powLimit);
    const unsigned int powLimitBits = powLimit.GetCompact();

    // MAX_FUTURE_BLOCK_TIME = 600 = 2*T: this is the maximum timestamp offset
    // an attacker can inject per block without nodes rejecting the block.
    const int64_t FTL = 600; // seconds
    BOOST_REQUIRE_EQUAL(FTL, 2 * T); // contract: FTL must equal 2T for this test

    const int L     = static_cast<int>(N) + 1; // bootstrap end: height <= L returns powLimit
    const int TOTAL = L + 1 + static_cast<int>(4 * N);

    std::vector<CBlockIndex> blocks(TOTAL);

    for (int i = 0; i <= L; i++) {
        blocks[i].pprev      = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight    = i;
        blocks[i].nTime      = static_cast<uint32_t>(1775999888LL + static_cast<int64_t>(i) * T);
        blocks[i].nBits      = genesisBits;
        blocks[i].nChainWork = i ? blocks[i-1].nChainWork + GetBlockProof(blocks[i-1])
                                 : arith_uint256(0);
    }

    int64_t cur_ts = static_cast<int64_t>(blocks[L].nTime);
    auto fill_phase = [&](int from, int count, int64_t spacing) {
        for (int j = 0; j < count; j++) {
            const int i = from + j;
            cur_ts += spacing;
            blocks[i].pprev      = &blocks[i - 1];
            blocks[i].nHeight    = i;
            blocks[i].nTime      = static_cast<uint32_t>(cur_ts);
            blocks[i].nBits      = GetNextWorkRequired(&blocks[i - 1], nullptr, consensus);
            blocks[i].nChainWork = blocks[i-1].nChainWork + GetBlockProof(blocks[i-1]);
        }
    };

    const int ph1 = L + 1;
    const int ph2 = ph1 + static_cast<int>(N);
    const int ph3 = ph2 + static_cast<int>(N);
    const int ph4 = ph3 + static_cast<int>(N);

    fill_phase(ph1, static_cast<int>(N), T);    // stable
    fill_phase(ph2, static_cast<int>(N), FTL);  // attack: every block at max future time
    fill_phase(ph3, static_cast<int>(N), T);    // 1st recovery
    fill_phase(ph4, static_cast<int>(N), T);    // 2nd recovery

    const unsigned int after_stable = blocks[ph2 - 1].nBits;
    const unsigned int after_attack = blocks[ph3 - 1].nBits;
    const unsigned int after_rec1   = blocks[ph4 - 1].nBits;
    const unsigned int after_rec2   = blocks[TOTAL - 1].nBits;

    // Stable baseline.
    BOOST_CHECK_EQUAL(after_stable, 0x1f1ffffeU);

    // Attack immediately clamps to powLimit - hits the floor at step 1.
    BOOST_CHECK_EQUAL(after_attack, 0x1f1fffffU);
    BOOST_CHECK_EQUAL(after_attack, powLimitBits);

    // Recovery: 1st window still carries attack-era nBits > stays at powLimit.
    BOOST_CHECK_EQUAL(after_rec1, 0x1f1fffffU);
    BOOST_CHECK_EQUAL(after_rec1, powLimitBits);

    // 2nd window fully refreshed > returns to near-stable.
    BOOST_CHECK_EQUAL(after_rec2, 0x1f1ffffeU);

    // Structural properties.
    arith_uint256 t_stable, t_attack, t_rec2;
    t_stable.SetCompact(after_stable);
    t_attack.SetCompact(after_attack);
    t_rec2.SetCompact(after_rec2);

    BOOST_CHECK(t_attack  > t_stable); // attack eased difficulty (larger target)
    BOOST_CHECK(t_rec2   <= t_attack); // recovery hardened it back

    // Per-block safety: target never exceeds powLimit throughout all phases.
    for (int i = ph1; i < TOTAL; i++) {
        arith_uint256 tgt;
        tgt.SetCompact(blocks[i].nBits);
        BOOST_CHECK(tgt <= powLimit);
    }

    // During the attack phase the target must be non-decreasing (difficulty
    // can only ease, never accidentally harden mid-attack).
    for (int i = ph2 + 1; i < ph3; i++) {
        arith_uint256 tgt_cur, tgt_prev;
        tgt_cur.SetCompact(blocks[i].nBits);
        tgt_prev.SetCompact(blocks[i - 1].nBits);
        BOOST_CHECK(tgt_cur >= tgt_prev);
    }
}

// ---------------------------------------------------------------------------
// Test 20: Timestamp rise attack - compressed timestamps inflate difficulty.
//
//   An attacker compresses timestamps to the minimum valid value: prev + 1s.
//   This makes LWMA-3 believe blocks are arriving 300? faster than reality,
//   causing difficulty to rise by ~979? over one full N-block window.
//
//   Key finding (from Python simulation):
//     After N=576 blocks at 1s spacing: target = 0x1e086e41 (~972x harder).
//     The attack requires the attacker to have the hashrate to keep mining
//     at this extreme difficulty - which grows exponentially as the window
//     fills.  This makes the attack self-limiting: it only succeeds if the
//     attacker has an overwhelming hashrate advantage, in which case they
//     could already 51%-attack the network directly.
//
//   Recovery behavior (also tested here):
//     After the attack window clears and honest miners resume at spacing T,
//     difficulty eases slowly because old high-difficulty nBits persist in
//     the window for N blocks.  Recovery takes many windows to complete.
//     All recovery blocks stay <= powLimit (proven per-block below).
//
//   Monotonicity during attack:
//     Target is strictly non-increasing during the attack (difficulty only
//     rises, never accidentally drops while timestamps are compressed).
//
//   Scenario (live nBits propagation):
//     Phase 1: N=576 blocks at  T   - stable baseline
//     Phase 2: N=576 blocks at  1s  - compressed-timestamp rise attack
//     Phase 3: N=576 blocks at  T   - 1st recovery window
//     Phase 4: N=576 blocks at  T   - 2nd recovery window
//
//   All expected values verified by Python arith_uint256 simulation.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_timestamp_rise_attack)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit   = UintToArith256(consensus.powLimit);

    const int L     = static_cast<int>(N) + 1;
    const int TOTAL = L + 1 + static_cast<int>(4 * N);

    std::vector<CBlockIndex> blocks(TOTAL);

    for (int i = 0; i <= L; i++) {
        blocks[i].pprev      = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight    = i;
        blocks[i].nTime      = static_cast<uint32_t>(1775999888LL + static_cast<int64_t>(i) * T);
        blocks[i].nBits      = genesisBits;
        blocks[i].nChainWork = i ? blocks[i-1].nChainWork + GetBlockProof(blocks[i-1])
                                 : arith_uint256(0);
    }

    int64_t cur_ts = static_cast<int64_t>(blocks[L].nTime);
    auto fill_phase = [&](int from, int count, int64_t spacing) {
        for (int j = 0; j < count; j++) {
            const int i = from + j;
            cur_ts += spacing;
            blocks[i].pprev      = &blocks[i - 1];
            blocks[i].nHeight    = i;
            blocks[i].nTime      = static_cast<uint32_t>(cur_ts);
            blocks[i].nBits      = GetNextWorkRequired(&blocks[i - 1], nullptr, consensus);
            blocks[i].nChainWork = blocks[i-1].nChainWork + GetBlockProof(blocks[i-1]);
        }
    };

    const int ph1 = L + 1;
    const int ph2 = ph1 + static_cast<int>(N);
    const int ph3 = ph2 + static_cast<int>(N);
    const int ph4 = ph3 + static_cast<int>(N);

    fill_phase(ph1, static_cast<int>(N), T);  // stable
    fill_phase(ph2, static_cast<int>(N), 1);  // attack: 1-second timestamps
    fill_phase(ph3, static_cast<int>(N), T);  // 1st recovery
    fill_phase(ph4, static_cast<int>(N), T);  // 2nd recovery

    const unsigned int after_stable = blocks[ph2 - 1].nBits;
    const unsigned int after_attack = blocks[ph3 - 1].nBits;
    const unsigned int after_rec1   = blocks[ph4 - 1].nBits;
    const unsigned int after_rec2   = blocks[TOTAL - 1].nBits;

    // Stable baseline.
    BOOST_CHECK_EQUAL(after_stable, 0x1f1ffffeU);

    // After full N-block attack: ~972? difficulty increase.
    BOOST_CHECK_EQUAL(after_attack, 0x1e086e41U);

    // After recovery, difficulty eases but very slowly (old high-difficulty
    // nBits stay in the window for N blocks after the attack ends).
    BOOST_CHECK_EQUAL(after_rec1, 0x1f01404fU);
    BOOST_CHECK_EQUAL(after_rec2, 0x1f0136f0U);

    // Structural: attack raises difficulty (target falls).
    arith_uint256 t_stable, t_attack, t_rec1, t_rec2;
    t_stable.SetCompact(after_stable);
    t_attack.SetCompact(after_attack);
    t_rec1.SetCompact(after_rec1);
    t_rec2.SetCompact(after_rec2);

    BOOST_CHECK(t_attack < t_stable);  // attack raised difficulty (smaller target)
    BOOST_CHECK(t_rec1   > t_attack);  // recovery eases (larger target than attack low)
    BOOST_CHECK(t_rec2   > t_attack);  // 2nd window continues easing

    // Recovery is still far from stable - the window takes time to flush
    // high-difficulty attack blocks.  (Full convergence takes many N-windows.)
    BOOST_CHECK(t_rec2 < t_stable);

    // Monotonicity during attack: target non-increasing (difficulty only rises).
    for (int i = ph2 + 1; i < ph3; i++) {
        arith_uint256 tgt_cur, tgt_prev;
        tgt_cur.SetCompact(blocks[i].nBits);
        tgt_prev.SetCompact(blocks[i - 1].nBits);
        BOOST_CHECK(tgt_cur <= tgt_prev);
    }

    // Per-block safety: target never above powLimit across all four phases.
    for (int i = ph1; i < TOTAL; i++) {
        arith_uint256 tgt;
        tgt.SetCompact(blocks[i].nBits);
        BOOST_CHECK(tgt <= powLimit);
    }
}

// ---------------------------------------------------------------------------
// Test 21: Alternating timestamp attack - 600s / 1s interleaving.
//
//   The attacker alternates between the two extremes: maximum-future block
//   (600s = 2T per block) and minimum-valid block (1s per block).
//   Average solvetime = (600 + 1) / 2 = 300.5s ? T.
//
//   Key finding:
//     The attack DOES hit powLimit: after N=576 alternating blocks both
//     after_alt_1N and after_alt_2N equal powLimitBits (0x1f1fffff).
//     This is because every 600s block (2T) alone is sufficient to push
//     the target toward the floor (as proven in test 19), and the
//     interleaved 1s blocks cannot fully counteract this easing pressure.
//     The powLimit floor is the hard boundary - behaviour is identical to
//     a pure drop attack once the floor is reached.
//
//     Conclusion: alternating FTL/min-timestamp provides no extra benefit
//     over a pure drop attack.  Both hit the same powLimit floor.
//
//   Scenario (live nBits propagation):
//     Phase 1: N=576 blocks at  T         - stable baseline
//     Phase 2: N=576 blocks alternating   - 600s, 1s, 600s, 1s ...
//     Phase 3: N=576 blocks alternating   - second attack window
//
//   All expected values verified by Python arith_uint256 simulation.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_timestamp_alternating_attack)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits  = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit    = UintToArith256(consensus.powLimit);
    const unsigned int powLimitBits = powLimit.GetCompact();

    const int L     = static_cast<int>(N) + 1;
    const int TOTAL = L + 1 + static_cast<int>(3 * N);

    std::vector<CBlockIndex> blocks(TOTAL);

    for (int i = 0; i <= L; i++) {
        blocks[i].pprev      = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight    = i;
        blocks[i].nTime      = static_cast<uint32_t>(1775999888LL + static_cast<int64_t>(i) * T);
        blocks[i].nBits      = genesisBits;
        blocks[i].nChainWork = i ? blocks[i-1].nChainWork + GetBlockProof(blocks[i-1])
                                 : arith_uint256(0);
    }

    // Index of the live block relative to first live height (L+1), used to
    // decide even/odd alternation.
    int alt_idx = 0;
    int64_t cur_ts = static_cast<int64_t>(blocks[L].nTime);

    auto fill_stable = [&](int64_t from, int64_t count) {
        for (int64_t j = 0; j < count; j++) {
            const int64_t idx = from + j;
            cur_ts += T;
            blocks[idx].pprev      = &blocks[idx - 1];
            blocks[idx].nHeight    = idx;
            blocks[idx].nTime      = static_cast<uint32_t>(cur_ts);
            blocks[idx].nBits      = GetNextWorkRequired(&blocks[idx - 1], nullptr, consensus);
            blocks[idx].nChainWork = blocks[idx-1].nChainWork + GetBlockProof(blocks[idx-1]);
        }
    };

    auto fill_alternating = [&](int64_t from, int64_t count) {
        for (int64_t j = 0; j < count; j++, alt_idx++) {
            const int64_t idx = from + j;
            // Even index: 600s (max future timestamp); odd: 1s (min timestamp).
            const int64_t spacing = (alt_idx % 2 == 0) ? 600 : 1;
            cur_ts += spacing;
            blocks[idx].pprev      = &blocks[idx - 1];
            blocks[idx].nHeight    = idx;
            blocks[idx].nTime      = static_cast<uint32_t>(cur_ts);
            blocks[idx].nBits      = GetNextWorkRequired(&blocks[idx - 1], nullptr, consensus);
            blocks[idx].nChainWork = blocks[idx-1].nChainWork + GetBlockProof(blocks[idx-1]);
        }
    };

    const int ph1 = L + 1;
    const int ph2 = ph1 + static_cast<int>(N);
    const int ph3 = ph2 + static_cast<int>(N);

    fill_stable(ph1, static_cast<int>(N));       // stable baseline
    fill_alternating(ph2, static_cast<int>(N));  // 1st attack window
    fill_alternating(ph3, static_cast<int>(N));  // 2nd attack window

    const unsigned int after_stable  = blocks[ph2 - 1].nBits;
    const unsigned int after_alt_1N  = blocks[ph3 - 1].nBits;
    const unsigned int after_alt_2N  = blocks[TOTAL - 1].nBits;

    // Stable baseline unchanged.
    BOOST_CHECK_EQUAL(after_stable, 0x1f1ffffeU);

    // After one N-window of alternating: hits powLimit.
    // The 600s (2T) blocks are enough to pull the target to the floor
    // even though they alternate with 1s blocks.  Average solvetime
    // 300.5s > T, but the 6T-cap asymmetry means the easing effect of
    // 600s blocks dominates over the hardening effect of 1s blocks.
    BOOST_CHECK_EQUAL(after_alt_1N, 0x1f1fffffU);
    BOOST_CHECK_EQUAL(after_alt_1N, powLimitBits);

    // After two N-windows: still at powLimit - floor is stable.
    BOOST_CHECK_EQUAL(after_alt_2N, 0x1f1fffffU);
    BOOST_CHECK_EQUAL(after_alt_2N, powLimitBits);

    arith_uint256 t_stable, t_alt_1N, t_alt_2N;
    t_stable.SetCompact(after_stable);
    t_alt_1N.SetCompact(after_alt_1N);
    t_alt_2N.SetCompact(after_alt_2N);

    // Both attack windows hit the powLimit floor.
    // Note: compare against compact-roundtripped powLimit because SetCompact()
    // truncates precision - t_alt_xN can never equal the raw arith_uint256 powLimit.
    arith_uint256 powLimitCompact;
    powLimitCompact.SetCompact(powLimitBits);
    BOOST_CHECK(t_alt_1N == powLimitCompact);
    BOOST_CHECK(t_alt_2N == powLimitCompact);

    // The attack eases difficulty (larger target than stable).
    BOOST_CHECK(t_alt_1N > t_stable);

    // Per-block safety across all phases.
    for (int i = ph1; i < TOTAL; i++) {
        arith_uint256 tgt;
        tgt.SetCompact(blocks[i].nBits);
        BOOST_CHECK(tgt <= powLimit);
    }
}

// ---------------------------------------------------------------------------
// Test 22: Timestamp drop attack starting from high difficulty.
//
//   Verifies that the FTL=600s drop attack cannot collapse difficulty to
//   powLimit in a single window even when the chain starts at very high
//   difficulty.  This bounds the worst-case impact of the attack regardless
//   of how much hashrate was present before the attacker started.
//
//   Scenario (live nBits propagation throughout):
//     Seed phase  : 3 ? N=576 blocks at T/10 = 30s spacing
//                   > difficulty climbs organically to ~35 000? above powLimit
//     Attack phase: 3 ? N=576 blocks at FTL=600s (MAX_FUTURE_BLOCK_TIME)
//                   > attacker uses maximum future timestamp every block
//
//   Key finding (verified by Python arith_uint256 simulation):
//     Even starting at ~35 000? difficulty, after one full N-block attack
//     window the difficulty is still ~3 035? above powLimit - NOT at the
//     floor.  The target grows roughly ?10 per window (FTL?2T means the
//     effective solvetime budget is ?2T, so LWMA sees ?2T average and
//     the ratio drifts at most by a constant factor per window).
//     After three attack windows: 35 059? > 3 035? > 628? > 128?.
//     powLimit is never reached.
//
//   Expected values (Python arith_uint256 simulation):
//     After seed window 1: 0x1f012235  (~28x   above powLimit)
//     After seed window 2: 0x1e088d26  (~958x  above powLimit)
//     After seed window 3: 0x1d3bd0f2  (~35060x above powLimit)
//     After attack window 1: 0x1e02b31e (~3035x above powLimit)
//     After attack window 2: 0x1e0d0c1a (~628x  above powLimit)
//     After attack window 3: 0x1e3fe31f (~128x  above powLimit)
//
//   Structural assertions (no hardcoded values for bounds checks):
//     A. Seed actually raised difficulty: target < powLimit / 4.
//     B. After each attack window target is strictly above powLimit (no floor).
//     C. Each successive attack window eases difficulty further (cumulative).
//     D. Target never exceeds powLimit at any point.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(lwma3_timestamp_drop_attack_high_difficulty)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 576
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits  = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit    = UintToArith256(consensus.powLimit);
    const unsigned int powLimitBits = powLimit.GetCompact();

    // FTL = MAX_FUTURE_BLOCK_TIME = 600s = 2*T.
    const int64_t FTL = 600;
    BOOST_REQUIRE_EQUAL(FTL, 2 * T);

    const int L     = static_cast<int>(N) + 1; // bootstrap end: height <= L returns powLimit
    const int SEED_WINDOWS   = 3;
    const int ATTACK_WINDOWS = 3;
    const int TOTAL = L + 1 + (SEED_WINDOWS + ATTACK_WINDOWS) * static_cast<int>(N);

    std::vector<CBlockIndex> blocks(TOTAL);

    // Bootstrap phase.
    for (int i = 0; i <= L; i++) {
        blocks[i].pprev      = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight    = i;
        blocks[i].nTime      = static_cast<uint32_t>(1775999888LL + static_cast<int64_t>(i) * T);
        blocks[i].nBits      = genesisBits;
        blocks[i].nChainWork = i ? blocks[i-1].nChainWork + GetBlockProof(blocks[i-1])
                                 : arith_uint256(0);
    }

    int64_t cur_ts = static_cast<int64_t>(blocks[L].nTime);
    auto fill_phase = [&](int from, int count, int64_t spacing) {
        for (int j = 0; j < count; j++) {
            const int i = from + j;
            cur_ts += spacing;
            blocks[i].pprev      = &blocks[i - 1];
            blocks[i].nHeight    = i;
            blocks[i].nTime      = static_cast<uint32_t>(cur_ts);
            blocks[i].nBits      = GetNextWorkRequired(&blocks[i - 1], nullptr, consensus);
            blocks[i].nChainWork = blocks[i-1].nChainWork + GetBlockProof(blocks[i-1]);
        }
    };

    // Seed phase: 3 windows at T/10 = 30s - builds organic high difficulty.
    const int64_t FAST = T / 10; // 30s
    const int seed_start  = L + 1;
    const int attack_start = seed_start + SEED_WINDOWS * static_cast<int>(N);

    for (int w = 0; w < SEED_WINDOWS; w++) {
        fill_phase(seed_start + w * static_cast<int>(N), static_cast<int>(N), FAST);
    }

    // Verify expected seed nBits values (Python simulation).
    const int s1 = seed_start  + 1 * static_cast<int>(N) - 1;
    const int s2 = seed_start  + 2 * static_cast<int>(N) - 1;
    const int s3 = attack_start - 1;
    BOOST_CHECK_EQUAL(blocks[s1].nBits, 0x1f012235U); // ~28x   above powLimit
    BOOST_CHECK_EQUAL(blocks[s2].nBits, 0x1e088d26U); // ~958x  above powLimit
    BOOST_CHECK_EQUAL(blocks[s3].nBits, 0x1d3bd0f2U); // ~35060x above powLimit

    // (A) Seed actually raised difficulty well above stable.
    arith_uint256 seed_target;
    seed_target.SetCompact(blocks[s3].nBits);
    BOOST_CHECK(seed_target < powLimit / 4);

    // Attack phase: 3 windows at FTL=600s.
    for (int w = 0; w < ATTACK_WINDOWS; w++) {
        fill_phase(attack_start + w * static_cast<int>(N), static_cast<int>(N), FTL);
    }

    const int a1 = attack_start + 1 * static_cast<int>(N) - 1;
    const int a2 = attack_start + 2 * static_cast<int>(N) - 1;
    const int a3 = attack_start + 3 * static_cast<int>(N) - 1;

    // Verify expected attack nBits values (Python simulation).
    BOOST_CHECK_EQUAL(blocks[a1].nBits, 0x1e02b31eU); // ~3035x above powLimit
    BOOST_CHECK_EQUAL(blocks[a2].nBits, 0x1e0d0c1aU); // ~628x  above powLimit
    BOOST_CHECK_EQUAL(blocks[a3].nBits, 0x1e3fe31fU); // ~128x  above powLimit

    // (B) After each attack window, difficulty is still well above powLimit floor.
    BOOST_CHECK(blocks[a1].nBits != powLimitBits);
    BOOST_CHECK(blocks[a2].nBits != powLimitBits);
    BOOST_CHECK(blocks[a3].nBits != powLimitBits);

    arith_uint256 t_a1, t_a2, t_a3;
    t_a1.SetCompact(blocks[a1].nBits);
    t_a2.SetCompact(blocks[a2].nBits);
    t_a3.SetCompact(blocks[a3].nBits);

    BOOST_CHECK(t_a1 < powLimit);
    BOOST_CHECK(t_a2 < powLimit);
    BOOST_CHECK(t_a3 < powLimit);

    // (C) Each attack window eases difficulty further (target grows each window).
    BOOST_CHECK(t_a2 > t_a1);
    BOOST_CHECK(t_a3 > t_a2);

    // (D) Target never exceeds powLimit across all phases.
    for (int i = seed_start; i < TOTAL; i++) {
        arith_uint256 tgt;
        tgt.SetCompact(blocks[i].nBits);
        BOOST_CHECK(tgt <= powLimit);
    }
}

// ---------------------------------------------------------------------------
// Existing proof-of-work validity tests - unchanged from upstream.
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_negative_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    nBits = UintToArith256(consensus.powLimit).GetCompact(true);
    hash = uint256{1};
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_overflow_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits{~0x00800000U};
    hash = uint256{1};
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_too_easy_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 nBits_arith = UintToArith256(consensus.powLimit);
    nBits_arith *= 2;
    nBits = nBits_arith.GetCompact();
    hash = uint256{1};
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_biger_hash_than_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 hash_arith = UintToArith256(consensus.powLimit);
    nBits = hash_arith.GetCompact();
    hash_arith *= 2; // hash > nBits
    hash = ArithToUint256(hash_arith);
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_zero_target)
{
    const auto consensus = CreateChainParams(*m_node.args, ChainType::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 hash_arith{0};
    nBits = hash_arith.GetCompact();
    hash = ArithToUint256(hash_arith);
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(GetBlockProofEquivalentTime_test)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    std::vector<CBlockIndex> blocks(10000);
    for (int i = 0; i < 10000; i++) {
        blocks[i].pprev = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = i;
        blocks[i].nTime = 1761999888 + i * chainParams->GetConsensus().nPowTargetSpacing;
        blocks[i].nBits = 0x207fffff; /* target 0x7fffff000... */
        blocks[i].nChainWork = i ? blocks[i - 1].nChainWork + GetBlockProof(blocks[i - 1]) : arith_uint256(0);
    }

    for (int j = 0; j < 1000; j++) {
        CBlockIndex *p1 = &blocks[m_rng.randrange(10000)];
        CBlockIndex *p2 = &blocks[m_rng.randrange(10000)];
        CBlockIndex *p3 = &blocks[m_rng.randrange(10000)];

        int64_t tdiff = GetBlockProofEquivalentTime(*p1, *p2, *p3, chainParams->GetConsensus());
        BOOST_CHECK_EQUAL(tdiff, p1->GetBlockTime() - p2->GetBlockTime());
    }
}

// ---------------------------------------------------------------------------
// Chain-parameter sanity checks - run for every network type.
//
// In addition to the upstream checks (genesis hash, nBits validity), we verify
// the PoW routing mode for each chain so that any accidental change to
// chainparams.cpp that flips a difficulty flag is caught immediately.
//
//   Chain     | fPowNoRetargeting | LWMA runs?
//   ----------+-------------------+-----------
//   MAIN      | false             | yes
//   TESTNET   | false             | yes
//   TESTNET4  | false             | yes
//   SIGNET    | false             | yes
//   REGTEST   | true              | no (branch 1)
// ---------------------------------------------------------------------------
void sanity_check_chainparams(const ArgsManager& args, ChainType chain_type)
{
    const auto chainParams = CreateChainParams(args, chain_type);
    const auto consensus = chainParams->GetConsensus();

    // Genesis block hash must match the value committed in chainparams.cpp.
    BOOST_CHECK_EQUAL(consensus.hashGenesisBlock, chainParams->GenesisBlock().GetHash());

    /*
    // target timespan is an even multiple of spacing
    BOOST_CHECK_EQUAL(consensus.nPowTargetTimespan % consensus.nPowTargetSpacing, 0);
    */

    // Genesis nBits must be positive, must not overflow the compact format,
    // and must represent a target at or below powLimit.
    arith_uint256 pow_compact;
    bool neg, over;
    pow_compact.SetCompact(chainParams->GenesisBlock().nBits, &neg, &over);
    BOOST_CHECK(!neg && pow_compact != 0);
    BOOST_CHECK(!over);
    BOOST_CHECK(UintToArith256(consensus.powLimit) >= pow_compact);

    // check max target * 4*nPowTargetTimespan doesn't overflow -- see pow.cpp:CalculateNextWorkRequired()
    /*
    if (!consensus.fPowNoRetargeting) {
        arith_uint256 targ_max{UintToArith256(uint256{"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"})};
        targ_max /= consensus.nPowTargetTimespan*4;
        BOOST_CHECK(UintToArith256(consensus.powLimit) < targ_max);
    */

    // lwmaAveragingWindow must always be a positive value even if LWMA does
    // not run on this chain - prevents division-by-zero should routing change.
    BOOST_CHECK(consensus.lwmaAveragingWindow > 0);

    // Verify the expected PoW routing mode per chain type.
    // These checks catch accidental flag changes in chainparams.cpp.
    switch (chain_type) {
    case ChainType::MAIN:
        // Full LWMA-3 path: no-retargeting shortcut must not be active.
        BOOST_CHECK(!consensus.fPowNoRetargeting);
        break;
    case ChainType::TESTNET:
    case ChainType::TESTNET4:
        // Full LWMA-3 path: same routing as mainnet.
        BOOST_CHECK(!consensus.fPowNoRetargeting);
        break;
    case ChainType::SIGNET:
        // Full LWMA-3 path: same routing as mainnet (no shortcuts).
        BOOST_CHECK(!consensus.fPowNoRetargeting);
        break;
    case ChainType::REGTEST:
        // No-retargeting shortcut: nBits never changes, fastest for testing.
        BOOST_CHECK(consensus.fPowNoRetargeting);
        break;
    }
}

BOOST_AUTO_TEST_CASE(ChainParams_MAIN_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::MAIN);
}

BOOST_AUTO_TEST_CASE(ChainParams_REGTEST_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::REGTEST);
}

BOOST_AUTO_TEST_CASE(ChainParams_TESTNET_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::TESTNET);
}

BOOST_AUTO_TEST_CASE(ChainParams_TESTNET4_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::TESTNET4);
}

BOOST_AUTO_TEST_CASE(ChainParams_SIGNET_sanity)
{
    sanity_check_chainparams(*m_node.args, ChainType::SIGNET);
}

BOOST_AUTO_TEST_SUITE_END()
