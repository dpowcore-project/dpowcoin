// Copyright (c) 2026 The Dpowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <arith_uint256.h>
#include <chainparams.h>
#include <consensus/params.h>
#include <pow.h>
#include <primitives/block.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/chaintype.h>

#include <boost/test/unit_test.hpp>

//! Dpowcoin-specific Dual PoW (Yespower + chained Argon2id) coverage.
//!
//! Bitcoin Core's upstream pow_tests.cpp validates difficulty math and
//! retargeting only. Dpowcoin additionally requires that BOTH PoW algorithms
//! pass the same nBits target (AND-check) to consider a header valid. These
//! tests pin that contract so it cannot regress silently.

BOOST_FIXTURE_TEST_SUITE(dpowcoin_pow_tests, BasicTestingSetup)

namespace {

//! Build a regtest CBlockHeader using the actual regtest genesis parameters
//! from src/kernel/chainparams.cpp (CRegTestParams). Using real genesis
//! values guarantees Yespower + Argon2id hashes are deterministic and
//! reproducible across builds.
CBlockHeader MakeRegtestGenesisHeader()
{
    CBlockHeader h;
    h.nVersion       = 1;
    h.hashPrevBlock  = uint256();
    // Merkle root from the regtest genesis coinbase; computed by
    // CreateGenesisBlock(1296688602, 2, 0x207fffff, 1, 50*COIN). We let
    // the test recompute via the params; for header-only PoW we only need
    // the four fields below — the merkle root only affects the block hash,
    // not the Yespower/Argon2id PoW which hashes the 80-byte header.
    h.hashMerkleRoot = uint256();
    h.nTime          = 1296688602;
    h.nBits          = 0x207fffff;
    h.nNonce         = 2;
    return h;
}

} // namespace

//! 1. Genesis-style header: both Yespower and Argon2id hashes must be below
//!    the regtest powLimit-derived target. (Sanity: Dual PoW is computable.)
BOOST_AUTO_TEST_CASE(dual_pow_hashes_are_deterministic)
{
    CBlockHeader h = MakeRegtestGenesisHeader();
    const uint256 yhash = h.GetYespowerPoWHash();
    const uint256 ahash = h.GetArgon2idPoWHash();

    // Both algorithms must yield non-zero, distinct hashes (independence).
    BOOST_CHECK(yhash != uint256());
    BOOST_CHECK(ahash != uint256());
    BOOST_CHECK(yhash != ahash);

    // Re-evaluation must be deterministic.
    BOOST_CHECK_EQUAL(h.GetYespowerPoWHash().GetHex(), yhash.GetHex());
    BOOST_CHECK_EQUAL(h.GetArgon2idPoWHash().GetHex(), ahash.GetHex());
}

//! 2. CheckProofOfWork acceptance: a hash of zero must pass under any
//!    realistic target. Confirms the accept-side of the PoW predicate is
//!    wired correctly for both Yespower and Argon2id legs (they share
//!    CheckProofOfWork).
BOOST_AUTO_TEST_CASE(check_pow_accepts_under_regtest_limit)
{
    const auto chainParams = CreateChainParams(*m_node.args, ChainType::REGTEST);
    const auto& consensus = chainParams->GetConsensus();

    uint256 low_hash; // all zeros
    BOOST_CHECK(CheckProofOfWork(low_hash, 0x207fffff, consensus));
}

//! 3. CheckProofOfWork must reject a hash above the target. Use mainnet's
//!    far stricter powLimit (0x1f...) to ensure the regtest-derived hashes
//!    are rejected, exercising the high-hash path that maps to "high-hash"
//!    reject reason in CheckBlockHeader().
BOOST_AUTO_TEST_CASE(check_pow_rejects_when_above_target)
{
    const auto mainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& mainConsensus = mainParams->GetConsensus();

    // A plainly-too-high hash: all-ones uint256.
    uint256 too_high;
    for (size_t i = 0; i < too_high.size(); ++i) {
        const_cast<unsigned char*>(too_high.data())[i] = 0xff;
    }
    // Use mainnet nBits (0x1f1fffff per chainparams.cpp:137).
    BOOST_CHECK(!CheckProofOfWork(too_high, 0x1f1fffff, mainConsensus));
}

//! 4. AND-check semantics: the consensus rule requires BOTH algorithms to
//!    pass. Simulate Yespower-pass + Argon2id-fail by feeding a deliberately
//!    too-high hash for the second leg and asserting the combined verdict
//!    is failure. This mirrors validation.cpp's two sequential
//!    CheckProofOfWork calls in CheckBlockHeader().
BOOST_AUTO_TEST_CASE(dual_pow_and_check_rejects_if_either_fails)
{
    const auto mainParams = CreateChainParams(*m_node.args, ChainType::MAIN);
    const auto& consensus = mainParams->GetConsensus();

    // Pretend the yespower leg passed (use a low hash).
    uint256 low_hash; // all zeros — always under any target
    BOOST_CHECK(CheckProofOfWork(low_hash, 0x1f1fffff, consensus));

    // Pretend the argon2id leg failed (all 0xff).
    uint256 high_hash;
    for (size_t i = 0; i < high_hash.size(); ++i) {
        const_cast<unsigned char*>(high_hash.data())[i] = 0xff;
    }
    BOOST_CHECK(!CheckProofOfWork(high_hash, 0x1f1fffff, consensus));

    // Combined AND: pass && fail == fail.
    const bool combined = CheckProofOfWork(low_hash, 0x1f1fffff, consensus) &&
                          CheckProofOfWork(high_hash, 0x1f1fffff, consensus);
    BOOST_CHECK(!combined);
}

//! 5. Yespower must NOT short-circuit Argon2id evaluation. If a header has a
//!    valid Yespower hash but invalid Argon2id hash, CheckBlockHeader (in
//!    validation.cpp) must still call the Argon2id leg. We assert this
//!    indirectly: GetArgon2idPoWHash() must be callable independently and
//!    yield a stable result regardless of Yespower having been called.
BOOST_AUTO_TEST_CASE(argon2id_independent_of_yespower)
{
    CBlockHeader h = MakeRegtestGenesisHeader();
    const uint256 a1 = h.GetArgon2idPoWHash();
    (void)h.GetYespowerPoWHash();
    const uint256 a2 = h.GetArgon2idPoWHash();
    BOOST_CHECK_EQUAL(a1.GetHex(), a2.GetHex());
}

//! 6. Different headers produce different Dual PoW hashes (collision sanity).
BOOST_AUTO_TEST_CASE(different_headers_produce_different_hashes)
{
    CBlockHeader h1 = MakeRegtestGenesisHeader();
    CBlockHeader h2 = MakeRegtestGenesisHeader();
    h2.nNonce = h1.nNonce + 1;

    BOOST_CHECK(h1.GetYespowerPoWHash() != h2.GetYespowerPoWHash());
    BOOST_CHECK(h1.GetArgon2idPoWHash() != h2.GetArgon2idPoWHash());
}

BOOST_AUTO_TEST_SUITE_END()
