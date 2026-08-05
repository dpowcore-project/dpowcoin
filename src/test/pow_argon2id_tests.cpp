// Copyright (c) 2026-present The Dpowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <crypto/argon2d/argon2.h>
#include <primitives/block.h>
#include <streams.h>
#include <uint256.h>
#include <util/strencodings.h>

BOOST_AUTO_TEST_SUITE(pow_argon2id_tests)

static CBlockHeader GenesisHeader()
{
    static const std::string HEX_HEADER =
        "010000000000000000000000000000000000000000000000000000000000000000000000"
        "b38e51e3f65b6692f716de315ebb197333fb1596b80b54cf9a4469516e37f510dae"
        "5494dffff7f2002000000";

    CBlockHeader hdr;
    DataStream ss{ParseHex(HEX_HEADER)};
    ss >> hdr;
    return hdr;
}

BOOST_AUTO_TEST_CASE(argon2id_genesis_known_vector)
{
    const CBlockHeader hdr = GenesisHeader();
    BOOST_CHECK_EQUAL(
        hdr.GetArgon2idPoWHash().ToString(),
        "0171a6a88e466a2655dd0e2e404725bd4261e2aa5437c7cec7af7a04a2a6a1e6");
}

BOOST_AUTO_TEST_CASE(argon2id_cross_isa_consistency)
{
    const CBlockHeader hdr = GenesisHeader();

    Argon2AutoDetect(argon2_implementation::STANDARD);
    const std::string hash_std = hdr.GetArgon2idPoWHash().ToString();

    Argon2AutoDetect(argon2_implementation::USE_SSE2);
    BOOST_CHECK_EQUAL(hdr.GetArgon2idPoWHash().ToString(), hash_std);

    Argon2AutoDetect(argon2_implementation::USE_SSSE3);
    BOOST_CHECK_EQUAL(hdr.GetArgon2idPoWHash().ToString(), hash_std);

    Argon2AutoDetect(argon2_implementation::USE_AVX2);
    BOOST_CHECK_EQUAL(hdr.GetArgon2idPoWHash().ToString(), hash_std);

    Argon2AutoDetect(argon2_implementation::USE_AVX512);
    BOOST_CHECK_EQUAL(hdr.GetArgon2idPoWHash().ToString(), hash_std);

    Argon2AutoDetect();
}

BOOST_AUTO_TEST_CASE(argon2id_determinism)
{
    const CBlockHeader hdr = GenesisHeader();
    const std::string h1 = hdr.GetArgon2idPoWHash().ToString();
    BOOST_CHECK_EQUAL(hdr.GetArgon2idPoWHash().ToString(), h1);
    BOOST_CHECK_EQUAL(hdr.GetArgon2idPoWHash().ToString(), h1);
}

BOOST_AUTO_TEST_CASE(argon2id_input_sensitivity)
{
    CBlockHeader hdr = GenesisHeader();
    const std::string hash_orig = hdr.GetArgon2idPoWHash().ToString();
    hdr.nNonce ^= 1;
    BOOST_CHECK(hdr.GetArgon2idPoWHash().ToString() != hash_orig);
}

BOOST_AUTO_TEST_SUITE_END()
