// Copyright (c) 2026-present The Dpowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <crypto/argon2d/argon2.h>
#include <test/data/argon2id_vectors.json.h>
#include <test/util/json.h>
#include <uint256.h>
#include <util/strencodings.h>

#include <algorithm>
#include <iostream>
#include <vector>

BOOST_AUTO_TEST_SUITE(crypto_argon2id_tests)

BOOST_AUTO_TEST_CASE(argon2id_json_vectors_all_isa)
{
    UniValue tests = read_json(json_tests::argon2id_vectors);
    BOOST_REQUIRE(!tests.isNull());
    BOOST_REQUIRE(tests.isArray());
    BOOST_REQUIRE_GT(tests.size(), 0);

    constexpr uint32_t t_cost = 3;
    constexpr uint32_t m_cost = 1024;
    constexpr uint32_t parallelism = 1;
    constexpr size_t   hash_len = 32;

    size_t failed_count = 0;

    for (size_t idx = 0; idx < tests.size(); ++idx) {
        const UniValue& vec = tests[idx];
        std::string data_hex = vec["data"].get_str();
        std::string salt_hex = vec["salt"].get_str();
        std::string expected_hex = vec["expected_hash"].get_str();

        std::vector<uint8_t> data = ParseHex(data_hex);
        std::vector<uint8_t> salt = ParseHex(salt_hex);
        std::vector<uint8_t> expected = ParseHex(expected_hex);
        BOOST_REQUIRE(!data.empty());
        BOOST_REQUIRE(!salt.empty());
        BOOST_REQUIRE(expected.size() == hash_len);

        auto check_isa = [&](argon2_implementation::UseImplementation impl, const std::string& name) {
            Argon2AutoDetect(impl);
            uint256 hash;
            int rc = argon2id_hash_raw(t_cost, m_cost, parallelism,
                                       data.data(), data.size(),
                                       salt.data(), salt.size(),
                                       hash.begin(), hash_len);
            BOOST_REQUIRE_MESSAGE(rc == ARGON2_OK, name << " failed at vector " << idx);
            std::vector<uint8_t> actual(hash.begin(), hash.end());
            if (!std::equal(actual.begin(), actual.end(), expected.begin())) {
                ++failed_count;
                std::cerr << "Vector " << idx << " " << name << " mismatch:\n"
                          << "  data: " << data_hex << "\n"
                          << "  salt: " << salt_hex << "\n"
                          << "  expected: " << expected_hex << "\n"
                          << "  actual:   " << HexStr(actual) << "\n";
            }
        };

        // Always test the STANDARD (reference) implementation.
        check_isa(argon2_implementation::STANDARD, "STANDARD");

        // Test x86 SIMD implementations only if they were compiled in.
#if defined(ENABLE_ARGON2_SSE2)
        check_isa(argon2_implementation::USE_SSE2, "SSE2");
#endif
#if defined(ENABLE_ARGON2_SSSE3)
        check_isa(argon2_implementation::USE_SSSE3, "SSSE3");
#endif
#if defined(ENABLE_ARGON2_AVX2)
        check_isa(argon2_implementation::USE_AVX2, "AVX2");
#endif
#if defined(ENABLE_ARGON2_AVX512)
        check_isa(argon2_implementation::USE_AVX512, "AVX512");
#endif

        // Test ARM NEON implementation only if compiled in.
#if defined(ENABLE_ARGON2_NEON)
        check_isa(argon2_implementation::USE_NEON, "NEON");
#endif
    }

    // Restore automatic ISA detection for subsequent tests.
    Argon2AutoDetect();

    BOOST_REQUIRE_EQUAL(failed_count, 0);
}

// Negative test: deliberately corrupt the expected hash and verify that
// the computed hash does *not* match. This guards against false positives.
BOOST_AUTO_TEST_CASE(argon2id_negative_test)
{
    UniValue tests = read_json(json_tests::argon2id_vectors);
    BOOST_REQUIRE(!tests.isNull());
    BOOST_REQUIRE(tests.isArray());
    BOOST_REQUIRE_GT(tests.size(), 0);

    const UniValue& vec = tests[0];
    std::vector<uint8_t> data = ParseHex(vec["data"].get_str());
    std::vector<uint8_t> salt = ParseHex(vec["salt"].get_str());
    std::vector<uint8_t> expected = ParseHex(vec["expected_hash"].get_str());
    BOOST_REQUIRE(!data.empty() && !salt.empty() && expected.size() == 32);

    // Corrupt the first byte of the expected hash.
    expected[0] ^= 0xFF;

    Argon2AutoDetect(argon2_implementation::STANDARD);
    uint256 hash;
    int rc = argon2id_hash_raw(3, 1024, 1,
                               data.data(), data.size(),
                               salt.data(), salt.size(),
                               hash.begin(), 32);
    BOOST_REQUIRE_EQUAL(rc, ARGON2_OK);

    std::vector<uint8_t> actual(hash.begin(), hash.end());
    BOOST_CHECK(actual != expected); // must not match

    Argon2AutoDetect();
}

BOOST_AUTO_TEST_SUITE_END()
