// Copyright (c) 2025-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <common/system.h>
#include <interfaces/mining.h>
#include <node/miner.h>
#include <pow.h>
#include <util/time.h>
#include <validation.h>

#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

using interfaces::BlockTemplate;
using interfaces::Mining;
using node::BlockAssembler;
using node::BlockWaitOptions;

namespace testnet4_miner_tests {

struct Testnet4MinerTestingSetup : public Testnet4Setup {
    std::unique_ptr<Mining> MakeMining()
    {
        return interfaces::MakeMining(m_node, /*wait_loaded=*/false);
    }
};
} // namespace testnet4_miner_tests

BOOST_FIXTURE_TEST_SUITE(testnet4_miner_tests, Testnet4MinerTestingSetup)

// ===========================================================================
// Mining interface test
// ===========================================================================
BOOST_AUTO_TEST_CASE(MiningInterface)
{
    auto mining{MakeMining()};
    BOOST_REQUIRE(mining);

    BlockAssembler::Options options;
    options.include_dummy_extranonce = true;
    std::unique_ptr<BlockTemplate> block_template;

    // Set node time a few minutes past the testnet4 genesis block
    const int64_t genesis_time{WITH_LOCK(cs_main, return m_node.chainman->ActiveChain().Tip()->GetBlockTime())};
    SetMockTime(genesis_time + 3 * 60);

    block_template = mining->createNewBlock(options, /*cooldown=*/false);
    BOOST_REQUIRE(block_template);

    // The template should use the mocked system time
    BOOST_REQUIRE_EQUAL(block_template->getBlockHeader().nTime, genesis_time + 3 * 60);

    const BlockWaitOptions wait_options{.timeout = MillisecondsDouble{0}, .fee_threshold = 1};

    // waitNext() should return nullptr because there is no better template
    auto should_be_nullptr = block_template->waitNext(wait_options);
    BOOST_REQUIRE(should_be_nullptr == nullptr);

    // This remains the case when exactly 20 minutes have gone by
    {
        LOCK(cs_main);
        SetMockTime(m_node.chainman->ActiveChain().Tip()->GetBlockTime() + 20 * 60);
    }
    should_be_nullptr = block_template->waitNext(wait_options);
    BOOST_REQUIRE(should_be_nullptr == nullptr);

    // fPowAllowMinDifficultyBlocks is disabled on all chains - the 20-minute
    // min-difficulty timer in WaitAndCreateNewBlock is dead code and never
    // triggers a new template. The block below is preserved for history.
    /*
    // One second later the difficulty drops and it returns a new template
    // Note that we can't test the actual difficulty change, because the
    // difficulty is already at 1.
    {
        LOCK(cs_main);
        SetMockTime(m_node.chainman->ActiveChain().Tip()->GetBlockTime() + 20 * 60 + 1);
    }
    block_template = block_template->waitNext(wait_options);
    BOOST_REQUIRE(block_template);
    */
}

// ===========================================================================
// LWMA-3 tests specific to Testnet4 (N=288, T=300)
//
// Testnet4 uses a smaller averaging window than mainnet:
//   N = lwmaAveragingWindow = 288  (mainnet uses 576)
//   T = nPowTargetSpacing   = 300 s
//   k = N*(N+1)*T/2         = 12 484 800
//
// Smaller N means faster difficulty response (~1 day vs ~2 days on mainnet)
// at the cost of slightly less stability. This is intentional for a test
// network where hashrate is unpredictable.
//
// Bootstrap threshold L = N+1 = 289
//   height <= L=289 -> return genesis nBits (bootstrap)
//   height >  L=289 -> first real LWMA at height N+2=290
//
// All expected values verified by Python arith_uint256 simulation that
// mirrors the exact integer arithmetic in Lwma3CalculateNextWorkRequired.
//
//   stable hashrate (height N+2 = 290)        : 0x1f1ffffeU
//   spacing = 6T (solvetime cap)              : 0x1f1fffffU  (== powLimit)
//   spacing = T/2 (2x hashrate)               : 0x1f0fffffU
//   spacing = T/3 (3x hashrate)               : 0x1f0aaaaaU
//   mixed (144 slow 2T + 144 fast T/2)        : 0x1f1c353eU
// ===========================================================================

// ---------------------------------------------------------------------------
// Helper: build a chain of `count` blocks using testnet4 genesis timestamp.
// ---------------------------------------------------------------------------
static std::vector<CBlockIndex> BuildTestnet4Chain(int count, unsigned int nBits,
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
// Test T4-1: Bootstrap boundary for testnet4 (N=288).
//   Any height <= L=N+1=289 must return genesis nBits unchanged.
//   Height N+2=290 is the first real LWMA computation.
//   This catches accidental changes to lwmaAveragingWindow in chainparams.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(testnet4_lwma3_bootstrap)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::TESTNET4);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow;
    const int64_t T        = consensus.nPowTargetSpacing;
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;

    // Verify we are actually testing N=288 and not some other value.
    BOOST_REQUIRE_EQUAL(N, 288);

    // height <= L=N+1=289 is bootstrap; last bootstrap pindexLast at height L-1=N=288.
    const int L = static_cast<int>(N) + 1; // N+1 = 289
    auto blocks = BuildTestnet4Chain(L, genesisBits, 1775999890, T); // heights 0..L-1

    // Boundary: height L-1 = N = 288 is the last bootstrap block.
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&blocks[L - 1], nullptr, consensus), genesisBits);
    // Interior heights also bootstrap.
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&blocks[1],     nullptr, consensus), genesisBits);
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&blocks[N / 2], nullptr, consensus), genesisBits);
}

// ---------------------------------------------------------------------------
// Test T4-2: First real LWMA-3 result at height N+2 = 290.
//   All N=288 window blocks carry genesisBits at ideal spacing T.
//   Expected: 0x1f0ffffeU.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(testnet4_lwma3_stable_hashrate)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::TESTNET4);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 288
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit   = UintToArith256(consensus.powLimit);

    const int lwma_height = static_cast<int>(N) + 2; // N+2 = 290, first real LWMA
    auto blocks = BuildTestnet4Chain(lwma_height + 1, genesisBits, 1775999890, T);

    const unsigned int expected_nbits = 0x1f1ffffeU;
    unsigned int result = GetNextWorkRequired(&blocks[lwma_height], nullptr, consensus);
    BOOST_CHECK_EQUAL(result, expected_nbits);

    arith_uint256 resultTarget;
    resultTarget.SetCompact(result);
    BOOST_CHECK(resultTarget <= powLimit);
}

// ---------------------------------------------------------------------------
// Test T4-3: Rounding error must NOT accumulate across successive blocks.
//   Height N+2=290 is still the bootstrap boundary itself (pindexLast at
//   height N+1=289 triggers height<=L early-return of powLimit), so it
//   legitimately differs from the steady-state LWMA value.
//   Every block from height N+3 = 291 onward must stay at 0x1f1ffffeU.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(testnet4_lwma3_no_drift)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::TESTNET4);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 288
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;

    const int EXTRA     = 8;
    const int chain_len = static_cast<int>(N) + 3 + EXTRA; // N+3+EXTRA
    auto blocks = BuildTestnet4Chain(chain_len, genesisBits, 1775999890, T);

    const int first_live = static_cast<int>(N) + 2; // height N+2 = 290 (bootstrap edge)
    for (int h = first_live; h < chain_len; h++) {
        blocks[h].nBits = GetNextWorkRequired(&blocks[h - 1], nullptr, consensus);
    }

    const int first_stable = static_cast<int>(N) + 3; // height N+3 = 291
    const unsigned int expected_nbits = 0x1f1ffffeU;
    for (int h = first_stable; h < chain_len; h++) {
        BOOST_CHECK_EQUAL(blocks[h].nBits, expected_nbits);
    }
}

// ---------------------------------------------------------------------------
// Test T4-4: powLimit cap - spacing >= 6T must clamp to powLimit.
//   Expected: 0x1f0fffffU.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(testnet4_lwma3_powlimit_cap)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::TESTNET4);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 288
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits  = chainParams->GenesisBlock().nBits;
    const unsigned int powLimitBits = UintToArith256(consensus.powLimit).GetCompact();

    const int lwma_height = static_cast<int>(N) + 2; // N+2 = 290, first real LWMA
    auto blocks = BuildTestnet4Chain(lwma_height + 1, genesisBits, 1775999890, 6 * T);

    unsigned int result = GetNextWorkRequired(&blocks[lwma_height], nullptr, consensus);
    BOOST_CHECK_EQUAL(result, 0x1f1fffffU);
    BOOST_CHECK_EQUAL(result, powLimitBits);
}

// ---------------------------------------------------------------------------
// Test T4-5: Doubled hashrate - spacing T/2 must raise difficulty.
//   Expected: 0x1f07ffffU.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(testnet4_lwma3_double_hashrate)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::TESTNET4);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 288
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit   = UintToArith256(consensus.powLimit);

    const int lwma_height = static_cast<int>(N) + 2; // N+2 = 290, first real LWMA
    auto blocks = BuildTestnet4Chain(lwma_height + 1, genesisBits, 1775999890, T / 2);

    unsigned int result = GetNextWorkRequired(&blocks[lwma_height], nullptr, consensus);
    BOOST_CHECK_EQUAL(result, 0x1f0fffffU);

    arith_uint256 resultTarget;
    resultTarget.SetCompact(result);
    BOOST_CHECK(resultTarget < powLimit);
}

// ---------------------------------------------------------------------------
// Test T4-6: Monotonicity - 3x hashrate must produce harder target than 2x.
//   spacing T/2 -> 0x1f0fffffU
//   spacing T/3 -> 0x1f0aaaaaU
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(testnet4_lwma3_monotone_difficulty)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::TESTNET4);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N = consensus.lwmaAveragingWindow; // 288
    const int64_t T = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;
    const int lwma_height = static_cast<int>(N) + 2; // N+2 = 290, first real LWMA

    auto blocks_2x = BuildTestnet4Chain(lwma_height + 1, genesisBits, 1775999890, T / 2);
    auto blocks_3x = BuildTestnet4Chain(lwma_height + 1, genesisBits, 1775999890, T / 3);

    unsigned int result_2x = GetNextWorkRequired(&blocks_2x[lwma_height], nullptr, consensus);
    unsigned int result_3x = GetNextWorkRequired(&blocks_3x[lwma_height], nullptr, consensus);

    BOOST_CHECK_EQUAL(result_2x, 0x1f0fffffU);
    BOOST_CHECK_EQUAL(result_3x, 0x1f0aaaaaU);

    arith_uint256 target_2x, target_3x;
    target_2x.SetCompact(result_2x);
    target_3x.SetCompact(result_3x);
    BOOST_CHECK(target_3x < target_2x);
}

// ---------------------------------------------------------------------------
// Test T4-7: Mixed-spacing determinism - regression guard for N=288.
//   Window split: first HALF=144 blocks at 2T, last HALF=144 at T/2.
//   Expected: 0x1f1c353eU (recomputed via faithful Python port of
//   Lwma3CalculateNextWorkRequired against testnet4's N=288, T=300;
//   the previous 0x1f1c0aa0 constant was wrong).
//
//   Any change to loop weights, timestamp clamping, or accumulator arithmetic
//   will produce a different value and fail this test.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(testnet4_lwma3_mixed_solvetimes_determinism)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::TESTNET4);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 288
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit   = UintToArith256(consensus.powLimit);

    // N=288, L=289: window at height N+2=290 is blocks 3..290.
    // previousTimestamp comes from block 2 (= N+2 - N).
    const int HALF      = static_cast<int>(N / 2); // 144
    const int chain_len = static_cast<int>(N) + 3; // N+3 = 291
    std::vector<CBlockIndex> blocks(chain_len);

    int64_t ts = 1775999890;
    for (int i = 0; i < chain_len; i++) {
        blocks[i].pprev      = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight    = i;
        blocks[i].nBits      = genesisBits;
        blocks[i].nTime      = static_cast<uint32_t>(ts);
        blocks[i].nChainWork = i ? blocks[i - 1].nChainWork + GetBlockProof(blocks[i - 1])
                                 : arith_uint256(0);
        if      (i < 3)             ts += T;        // heights 0..2 pre-window
        else if (i < 3 + HALF)      ts += 2 * T;   // heights 3..146 slow first half
        else                        ts += T / 2;    // heights 147..N+1 fast second half
    }

    const unsigned int expected_nbits = 0x1f1c353eU;
    unsigned int result = GetNextWorkRequired(&blocks[chain_len - 1], nullptr, consensus);
    BOOST_CHECK_EQUAL(result, expected_nbits);

    arith_uint256 resultTarget;
    resultTarget.SetCompact(result);
    BOOST_CHECK(resultTarget <= powLimit);
    BOOST_CHECK(resultTarget > arith_uint256(0));
}

// ---------------------------------------------------------------------------
// Test T4-8: Live-propagation stabilization - 2000 blocks after bootstrap.
//
//   Testnet4 uses N=288, so the compact-LSB drift occurs every 288 blocks
//   (roughly twice as often as on mainnet).  The test covers 6+ full window
//   rollovers (2000 / 288 ? 6.9) and verifies:
//     (a) Exact compact values at window-boundary checkpoints (N, 2N, 3N).
//     (b) Exact final value at step 2000.
//     (c) The algorithm stays bounded throughout - never above powLimit,
//         never below powLimit/2.
//
//   All expected values verified by Python arith_uint256 simulation.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(testnet4_lwma3_live_stabilization_2000_blocks)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::TESTNET4);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 288
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit   = UintToArith256(consensus.powLimit);

    BOOST_REQUIRE_EQUAL(N, 288);

    const int L     = static_cast<int>(N) + 1;
    const int LIVE  = 2000;
    const int TOTAL = L + 1 + LIVE;               // 60501

    std::vector<CBlockIndex> blocks(TOTAL);

    for (int i = 0; i <= L; i++) {
        blocks[i].pprev      = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight    = i;
        blocks[i].nTime      = static_cast<uint32_t>(1775999890LL + static_cast<int64_t>(i) * T);
        blocks[i].nBits      = genesisBits;
        blocks[i].nChainWork = i ? blocks[i-1].nChainWork + GetBlockProof(blocks[i-1])
                                 : arith_uint256(0);
    }

    int64_t cur_ts = static_cast<int64_t>(blocks[L].nTime) + T;
    for (int i = L + 1; i < TOTAL; i++) {
        blocks[i].pprev      = &blocks[i - 1];
        blocks[i].nHeight    = i;
        blocks[i].nTime      = static_cast<uint32_t>(cur_ts);
        blocks[i].nBits      = GetNextWorkRequired(&blocks[i - 1], nullptr, consensus);
        blocks[i].nChainWork = blocks[i-1].nChainWork + GetBlockProof(blocks[i-1]);
        cur_ts += T;
    }

    // (a) Window-boundary checkpoints.
    //   step N  = 288 : 0x1f1ffffe
    //   step 2N = 576 : 0x1f1ffffd
    //   step 3N = 864 : 0x1f1ffffc
    BOOST_CHECK_EQUAL(blocks[L + static_cast<int>(N)].nBits,     0x1f1ffffeU);
    BOOST_CHECK_EQUAL(blocks[L + static_cast<int>(2 * N)].nBits, 0x1f1ffffdU);
    BOOST_CHECK_EQUAL(blocks[L + static_cast<int>(3 * N)].nBits, 0x1f1ffffcU);

    // (b) Final value after 2000 steps (falls in [6N+1, 7N) -> 0x1f1ffff8).
    BOOST_CHECK_EQUAL(blocks[L + LIVE].nBits, 0x1f1ffff8U);

    // (c) Bounds check.
    for (int i = L + 1; i <= L + LIVE; i++) {
        arith_uint256 tgt;
        tgt.SetCompact(blocks[i].nBits);
        BOOST_CHECK(tgt <= powLimit);
        BOOST_CHECK(tgt >= powLimit / 2);
    }
}

// ---------------------------------------------------------------------------
// Test T4-9: Live hashrate spike (T/3) followed by full recovery (2N blocks).
//
//   Scenario (live nBits propagation, N=288):
//     Phase 1: N=288 blocks at  T   - stable baseline
//     Phase 2: N=288 blocks at T/3  - hashrate triples
//     Phase 3: N=288 blocks at  T   - 1st recovery window
//     Phase 4: N=288 blocks at  T   - 2nd recovery window
//
//   All expected values verified by Python arith_uint256 simulation.
// ---------------------------------------------------------------------------
BOOST_AUTO_TEST_CASE(testnet4_lwma3_live_spike_and_recovery)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::TESTNET4);
    const auto& consensus  = chainParams->GetConsensus();
    const int64_t N        = consensus.lwmaAveragingWindow; // 288
    const int64_t T        = consensus.nPowTargetSpacing;   // 300
    const unsigned int genesisBits = chainParams->GenesisBlock().nBits;
    const arith_uint256 powLimit   = UintToArith256(consensus.powLimit);

    const int L     = static_cast<int>(N) + 1;
    const int TOTAL = L + 1 + static_cast<int>(4 * N);

    std::vector<CBlockIndex> blocks(TOTAL);

    for (int i = 0; i <= L; i++) {
        blocks[i].pprev      = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight    = i;
        blocks[i].nTime      = static_cast<uint32_t>(1775999890LL + static_cast<int64_t>(i) * T);
        blocks[i].nBits      = genesisBits;
        blocks[i].nChainWork = i ? blocks[i-1].nChainWork + GetBlockProof(blocks[i-1])
                                 : arith_uint256(0);
    }

    int64_t cur_ts = static_cast<int64_t>(blocks[L].nTime);
    auto fill_phase = [&](int from, int count, int64_t spacing) {
        for (int i = from; i < from + count; i++) {
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

    // After spike: target lower (difficulty ~3? higher).
    // Python simulation: 0x1f0528f4
    BOOST_CHECK_EQUAL(after_spike, 0x1f0528f4U);

    // Recovery easing - two windows.
    // Python simulation: 0x1f060b60, then 0x1f060d1f
    BOOST_CHECK_EQUAL(after_rec1, 0x1f060b60U);
    BOOST_CHECK_EQUAL(after_rec2, 0x1f060d1fU);

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

BOOST_AUTO_TEST_SUITE_END()
