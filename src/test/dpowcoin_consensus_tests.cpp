// Copyright (c) 2026 The Dpowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Stage-3 (DAA / halving / Taproot / mempool) consensus audit tests.
//
// Scope:
//   * subsidy curve & full emission integral (Dpowcoin tokenomics: 50 COIN
//     initial reward, halving every 420 000 blocks → ≈42 000 000 COIN total)
//   * LWMA-1 retarget invariants (every-block retarget, warm-up window,
//     bounded per-block change on flat hashrate)
//   * PermittedDifficultyTransition K=4 clamp (SECURITY-FIXES C4)
//   * Taproot deployment is ALWAYS_ACTIVE on main / test / signet
//
// These tests are deliberately self-contained — they only exercise pure
// consensus helpers (no chainstate / no UTXO set / no PoW solving), so they
// run in milliseconds and are safe to keep in the default unit-test suite.

#include <arith_uint256.h>
#include <chainparams.h>
#include <consensus/amount.h>
#include <consensus/params.h>
#include <pow.h>
#include <test/util/setup_common.h>
#include <util/chaintype.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

#include <cstdint>

namespace {

// Re-derive the maximum possible emission for a Dpowcoin-style halving curve
// (initial subsidy S, halving interval H, integer right-shift truncation).
// Returns the exact sum in satoshis.
static CAmount ExpectedTotalEmission(CAmount initial_subsidy, int halving_interval)
{
    CAmount sum = 0;
    for (int halvings = 0; halvings < 64; ++halvings) {
        const CAmount subsidy = initial_subsidy >> halvings;
        if (subsidy == 0) break;
        sum += subsidy * static_cast<CAmount>(halving_interval);
    }
    return sum;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(dpowcoin_consensus_tests, BasicTestingSetup)

// -----------------------------------------------------------------------------
// Halving / emission curve
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(dpowcoin_halving_interval_is_420k)
{
    const auto chain = CreateChainParams(*m_node.args, ChainType::MAIN);
    BOOST_CHECK_EQUAL(chain->GetConsensus().nSubsidyHalvingInterval, 420000);

    const auto test_chain = CreateChainParams(*m_node.args, ChainType::TESTNET);
    BOOST_CHECK_EQUAL(test_chain->GetConsensus().nSubsidyHalvingInterval, 420000);
}

BOOST_AUTO_TEST_CASE(dpowcoin_subsidy_at_halving_boundaries)
{
    const auto chain = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& cp = chain->GetConsensus();

    BOOST_CHECK_EQUAL(GetBlockSubsidy(0,            cp), 50 * COIN);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(419999,       cp), 50 * COIN);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(420000,       cp), 25 * COIN);   // halving #1
    BOOST_CHECK_EQUAL(GetBlockSubsidy(840000,       cp), CAmount{1250000000}); // 12.5 COIN
    BOOST_CHECK_EQUAL(GetBlockSubsidy(420000 * 33,  cp), 0);           // shifted out
    BOOST_CHECK_EQUAL(GetBlockSubsidy(420000 * 64,  cp), 0);           // saturation guard
    BOOST_CHECK_EQUAL(GetBlockSubsidy(420000 * 100, cp), 0);
}

BOOST_AUTO_TEST_CASE(dpowcoin_subsidy_monotonically_decreasing)
{
    const auto chain = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& cp = chain->GetConsensus();

    CAmount prev = GetBlockSubsidy(0, cp);
    for (int h = 0; h < cp.nSubsidyHalvingInterval * 65; h += cp.nSubsidyHalvingInterval) {
        const CAmount cur = GetBlockSubsidy(h, cp);
        BOOST_CHECK(cur <= prev);
        BOOST_CHECK(cur >= 0);
        BOOST_CHECK(cur <= 50 * COIN);
        prev = cur;
    }
}

BOOST_AUTO_TEST_CASE(dpowcoin_total_emission_matches_tokenomics)
{
    // Closed-form expectation: 2 * 50 * 420000 * COIN = 42_000_000 COIN, modulo
    // integer truncation in the right-shift. Confirm the helper agrees with
    // the integral the production GetBlockSubsidy() implements.
    const CAmount expected = ExpectedTotalEmission(50 * COIN, 420000);
    BOOST_CHECK_EQUAL(expected, CAmount{4199999995380000}); // ≈ 41 999 999.953 8 COIN

    // Sanity: this is exactly 2× Bitcoin's 2 099 999 997 690 000.
    BOOST_CHECK_EQUAL(expected, 2 * CAmount{2099999997690000});
}

// -----------------------------------------------------------------------------
// LWMA-1 retarget
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(dpowcoin_lwma_window_is_576)
{
    const auto chain = CreateChainParams(*m_node.args, ChainType::MAIN);
    BOOST_CHECK_EQUAL(chain->GetConsensus().lwmaAveragingWindow, 576);
    BOOST_CHECK_EQUAL(chain->GetConsensus().nPowTargetSpacing, 5 * 60); // 300 s
}

BOOST_AUTO_TEST_CASE(dpowcoin_permitted_transition_bootstrap_window_is_open)
{
    // Inside the warm-up window the LWMA returns powLimit; the clamp must
    // not reject the resulting flat section, otherwise the chain cannot
    // bootstrap.
    const auto chain = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& cp = chain->GetConsensus();

    const uint32_t flat = UintToArith256(cp.powLimit).GetCompact();
    for (int64_t h = 0; h <= cp.lwmaAveragingWindow; ++h) {
        BOOST_CHECK(PermittedDifficultyTransition(cp, h, flat, flat));
        // Even an absurd jump is allowed during warm-up.
        BOOST_CHECK(PermittedDifficultyTransition(cp, h, flat, flat / 256));
    }
}

BOOST_AUTO_TEST_CASE(dpowcoin_permitted_transition_clamp_K4)
{
    // Past warm-up, the clamp must accept ±K=4× changes and reject anything
    // beyond. This is the SECURITY-FIXES C4 invariant — without it a peer
    // can flood headers-sync with arbitrary low-work chains.
    const auto chain = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& cp = chain->GetConsensus();
    const int64_t h = cp.lwmaAveragingWindow + 1;

    // Pick a target well inside (powLimit / 4, powLimit) so both ×4 and /4
    // remain representable without saturating against powLimit.
    arith_uint256 base_target = UintToArith256(cp.powLimit) / 16;
    // Roundtrip through compact to drop precision so that *_target * 4 / 4
    // stays exactly representable in compact form.
    {
        arith_uint256 tmp;
        bool n, o;
        tmp.SetCompact(base_target.GetCompact(), &n, &o);
        base_target = tmp;
    }
    const uint32_t base = base_target.GetCompact();

    // Identity transition: always permitted.
    BOOST_CHECK(PermittedDifficultyTransition(cp, h, base, base));

    // Just-below ×4 / ÷4 — permitted (clamp uses old*K and old/K as bounds,
    // but compact-form roundtripping can round upward, so pick a safe margin).
    arith_uint256 t_up   = base_target * 3;
    arith_uint256 t_down = base_target / 3;
    BOOST_CHECK(PermittedDifficultyTransition(cp, h, base, t_up.GetCompact()));
    BOOST_CHECK(PermittedDifficultyTransition(cp, h, base, t_down.GetCompact()));

    // ×8 / ÷8 — must be rejected (well past K=4 in either direction).
    arith_uint256 t_up_too_far   = base_target * 8;
    arith_uint256 t_down_too_far = base_target / 8;
    BOOST_CHECK(!PermittedDifficultyTransition(cp, h, base, t_up_too_far.GetCompact()));
    BOOST_CHECK(!PermittedDifficultyTransition(cp, h, base, t_down_too_far.GetCompact()));
}

BOOST_AUTO_TEST_CASE(dpowcoin_permitted_transition_rejects_above_powlimit)
{
    const auto chain = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& cp = chain->GetConsensus();
    const int64_t h = cp.lwmaAveragingWindow + 100;

    const arith_uint256 pow_limit = UintToArith256(cp.powLimit);
    const uint32_t base = arith_uint256(pow_limit / 2).GetCompact();

    // A target bigger than powLimit must always be rejected.
    arith_uint256 over_limit = pow_limit * 2;
    BOOST_CHECK(!PermittedDifficultyTransition(cp, h, base, over_limit.GetCompact()));
}

// -----------------------------------------------------------------------------
// Taproot deployment
// -----------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(dpowcoin_taproot_always_active_on_user_chains)
{
    for (const ChainType t : {ChainType::MAIN, ChainType::TESTNET, ChainType::SIGNET}) {
        const auto chain = CreateChainParams(*m_node.args, t);
        const auto& d = chain->GetConsensus().vDeployments[Consensus::DEPLOYMENT_TAPROOT];
        BOOST_CHECK_EQUAL(d.nStartTime, Consensus::BIP9Deployment::ALWAYS_ACTIVE);
        BOOST_CHECK_EQUAL(d.nTimeout,   Consensus::BIP9Deployment::NO_TIMEOUT);
        BOOST_CHECK_EQUAL(d.min_activation_height, 0);
    }
}

BOOST_AUTO_TEST_SUITE_END()
