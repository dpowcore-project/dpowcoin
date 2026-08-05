// Copyright (c) 2026-present The Dpowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/argon2d/argon2.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <test/fuzz/util.h>

#include <cstdint>
#include <vector>

namespace {

// Minimum salt length required by Argon2 (8 bytes).
constexpr size_t MIN_SALT_LEN = 8;

// Helper to call argon2id_hash_raw with a specific ISA implementation.
std::vector<uint8_t> RunArgon2idHash(argon2_implementation::UseImplementation impl,
                                     const std::vector<uint8_t>& data,
                                     const std::vector<uint8_t>& salt)
{
    Argon2AutoDetect(impl);
    std::vector<uint8_t> out(32);
    int rc = argon2id_hash_raw(
        /* t_cost */ 3,
        /* m_cost */ 1024,
        /* parallelism */ 1,
        data.data(), data.size(),
        salt.data(), salt.size(),
        out.data(), out.size()
    );
    assert(rc == ARGON2_OK);
    return out;
}

} // namespace

FUZZ_TARGET(crypto_argon2id)
{
    FuzzedDataProvider fdp{buffer.data(), buffer.size()};

    // Generate random input data and salt.
    std::vector<uint8_t> data = ConsumeRandomLengthByteVector(fdp, /* max_length */ 256);
    std::vector<uint8_t> salt = ConsumeRandomLengthByteVector(fdp, /* max_length */ 256);

    // Ensure salt is at least 8 bytes.
    if (salt.size() < MIN_SALT_LEN) {
        salt.resize(MIN_SALT_LEN);
        for (size_t i = 0; i < MIN_SALT_LEN; ++i) {
            salt[i] = fdp.ConsumeIntegral<uint8_t>();
        }
    }

    // Compute reference hash using STANDARD implementation.
    std::vector<uint8_t> hash_ref = RunArgon2idHash(argon2_implementation::STANDARD, data, salt);

    // Helper to compare another ISA against the reference.
    auto check_isa = [&](argon2_implementation::UseImplementation impl) {
        std::vector<uint8_t> hash = RunArgon2idHash(impl, data, salt);
        assert(hash == hash_ref);
    };

    // Test x86 SIMD implementations only if compiled in.
#if defined(ENABLE_ARGON2_SSE2)
    check_isa(argon2_implementation::USE_SSE2);
#endif
#if defined(ENABLE_ARGON2_SSSE3)
    check_isa(argon2_implementation::USE_SSSE3);
#endif
#if defined(ENABLE_ARGON2_AVX2)
    check_isa(argon2_implementation::USE_AVX2);
#endif
#if defined(ENABLE_ARGON2_AVX512)
    check_isa(argon2_implementation::USE_AVX512);
#endif

    // Test ARM NEON implementation only if compiled in.
#if defined(ENABLE_ARGON2_NEON)
    check_isa(argon2_implementation::USE_NEON);
#endif

    // Restore automatic ISA detection for subsequent fuzz iterations.
    Argon2AutoDetect();
}
