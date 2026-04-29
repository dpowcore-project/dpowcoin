// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_AMOUNT_H
#define BITCOIN_CONSENSUS_AMOUNT_H

#include <cstdint>

/** Amount in satoshis (Can be negative) */
typedef int64_t CAmount;

/** The amount of satoshis in one BTC. */
static constexpr CAmount COIN = 100000000;

/** No amount larger than this (in satoshi) is valid.
 *
 * Note that this constant is *not* the total money supply, which in Bitcoin
 * currently happens to be less than 21,000,000 BTC for various reasons, but
 * rather a sanity check. As this sanity check is used by consensus-critical
 * validation code, the exact value of the MAX_MONEY constant is consensus
 * critical; in unusual circumstances like a(nother) overflow bug that allowed
 * for the creation of coins out of thin air modification could lead to a fork.
 * */
// Dpowcoin tokenomics note (Stage-3 DAA/halving audit, 2026-04):
//   * initial subsidy : 50 COIN
//   * halving interval: 420 000 blocks (mainnet) -- twice Bitcoin's 210 000
//   * total emission  : ≈ 42 000 000 COIN (vs Bitcoin's ≈ 21M)
// MAX_MONEY is *kept at 21M* for now: per-output amounts on a 50-COIN-reward
// chain can never realistically approach 21M, so raising the cap would be a
// gratuitous consensus-loosening with no observable upside. The cumulative
// supply (which exceeds MoneyRange) is only relevant in tests/aggregations,
// not in any per-output or per-tx validation path -- those callers must
// therefore *not* feed cumulative supply into MoneyRange(). See
// src/test/validation_tests.cpp::subsidy_limit_test for the corrected
// pattern (assert nSubsidy <= 50 * COIN, not MoneyRange(nSum)).
static constexpr CAmount MAX_MONEY = 21000000 * COIN;
inline bool MoneyRange(const CAmount& nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

#endif // BITCOIN_CONSENSUS_AMOUNT_H
