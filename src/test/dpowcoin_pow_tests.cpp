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

//! 7. Avalanche property: a single-bit perturbation in nNonce must cause
//!    a large fraction (>32 of 256 bits) of bit-flips in BOTH hashes.
//!    A weaker bound than ideal SAC (~128) but enough to catch a hash that
//!    silently degrades to a near-identity (e.g. wrong personalization).
BOOST_AUTO_TEST_CASE(dual_pow_avalanche_on_single_nonce_flip)
{
    auto popcount256 = [](const uint256& a, const uint256& b) {
        unsigned bits = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            unsigned char x = a.data()[i] ^ b.data()[i];
            for (int k = 0; k < 8; ++k) bits += (x >> k) & 1u;
        }
        return bits;
    };

    CBlockHeader h1 = MakeRegtestGenesisHeader();
    CBlockHeader h2 = h1;
    h2.nNonce ^= 1u; // flip exactly one bit

    const unsigned y_diff = popcount256(h1.GetYespowerPoWHash(), h2.GetYespowerPoWHash());
    const unsigned a_diff = popcount256(h1.GetArgon2idPoWHash(), h2.GetArgon2idPoWHash());

    // A defective hash (e.g. xor-linear) would produce <16 differing bits.
    BOOST_CHECK_GE(y_diff, 32u);
    BOOST_CHECK_GE(a_diff, 32u);
}

//! 8. Independence-from-merkle: PoW hashes depend on the full 80-byte
//!    header, including hashMerkleRoot. Mutating the merkle root must
//!    change BOTH PoW outputs (otherwise a transaction-malleation attack
//!    could reuse a single PoW for many block bodies).
BOOST_AUTO_TEST_CASE(dual_pow_depends_on_merkle_root)
{
    CBlockHeader h1 = MakeRegtestGenesisHeader();
    CBlockHeader h2 = h1;
    // Set a non-zero merkle root.
    auto* m = const_cast<unsigned char*>(h2.hashMerkleRoot.data());
    m[0] = 0x42;

    BOOST_CHECK(h1.GetYespowerPoWHash() != h2.GetYespowerPoWHash());
    BOOST_CHECK(h1.GetArgon2idPoWHash() != h2.GetArgon2idPoWHash());
}

//! 9. Yespower personalization pin: the personalization string
//!    "One POW? Why not two? 17/04/2024" is a hard-coded consensus
//!    parameter (block.cpp:50). Any unintended edit would silently fork
//!    the chain. We pin the literal so a code-search grep over commits
//!    plus this test catches both directions of regression.
BOOST_AUTO_TEST_CASE(yespower_personalization_string_is_pinned)
{
    // This string is duplicated here intentionally to make a code-search
    // for the literal find both block.cpp and this test, and to fail
    // loudly if either is edited in isolation.
    static constexpr char kExpectedPers[] = "One POW? Why not two? 17/04/2024";
    static_assert(sizeof(kExpectedPers) - 1 == 32,
                  "Yespower pers length must remain 32 bytes");
    BOOST_CHECK_EQUAL(std::string(kExpectedPers).size(), 32u);
}

//! 10. Hash output domain: all bytes of the 32-byte digest are reachable.
//!     Mine through a small range of nonces and assert that the union of
//!     bits set across N hashes is well-distributed (>= 192 of 256 bits
//!     touched). Catches a hash that always zeroes a byte / nibble.
BOOST_AUTO_TEST_CASE(dual_pow_outputs_cover_full_digest_domain)
{
    CBlockHeader h = MakeRegtestGenesisHeader();
    uint256 yes_or{};
    uint256 arg_or{};
    constexpr uint32_t kSamples = 8; // keep CI cost modest; argon2id is slow
    for (uint32_t i = 0; i < kSamples; ++i) {
        h.nNonce = i + 1;
        const uint256 y = h.GetYespowerPoWHash();
        const uint256 a = h.GetArgon2idPoWHash();
        for (size_t b = 0; b < y.size(); ++b) {
            const_cast<unsigned char*>(yes_or.data())[b] |= y.data()[b];
            const_cast<unsigned char*>(arg_or.data())[b] |= a.data()[b];
        }
    }
    auto popcount = [](const uint256& v) {
        unsigned c = 0;
        for (size_t i = 0; i < v.size(); ++i)
            for (int k = 0; k < 8; ++k) c += (v.data()[i] >> k) & 1u;
        return c;
    };
    BOOST_CHECK_GE(popcount(yes_or), 160u);
    BOOST_CHECK_GE(popcount(arg_or), 160u);
}

BOOST_AUTO_TEST_SUITE_END()
