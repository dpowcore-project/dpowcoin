// Copyright (c) 2021-2026 The Dpowcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <crypto/argon2d/argon2.h>
#include <crypto/sha512.h>
#include <tinyformat.h>
#include <uint256.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

/* Consensus-critical parameters - must match block.cpp exactly */
static constexpr uint32_t ARGON2ID_T       = 2;
static constexpr uint32_t ARGON2ID_M1      = 4096;   /* round 1 memory cost (KiB) */
static constexpr uint32_t ARGON2ID_M2      = 32768;  /* round 2 memory cost (KiB) */
static constexpr uint32_t ARGON2ID_P       = 2;
static constexpr size_t   ARGON2ID_HASHLEN = 32;
static constexpr size_t   HEADER_LEN       = 80;

/*
 * Run one full two-round Argon2id PoW hash of a synthetic 80-byte block header.
 * Mirrors GetArgon2idPoWHash() in block.cpp exactly:
 *   salt  = SHA-512( SHA-512( header ) )
 *   hash  = Argon2id( pwd=header, salt=salt,  t=2, m=4096,  p=2 )  [round 1]
 *   hash2 = Argon2id( pwd=header, salt=hash,  t=2, m=32768, p=2 )  [round 2]
 */
static void RunArgon2idHash(benchmark::Bench& bench)
{
    std::array<uint8_t, HEADER_LEN> hdr{};
    hdr[0] = 0x04;

    bench.unit("hash").batch(1).run([&] {
        // Derive salt: SHA-512( SHA-512( header ) )
        std::vector<unsigned char> salt_sha512(CSHA512::OUTPUT_SIZE);
        CSHA512 sha512;
        sha512.Write(hdr.data(), hdr.size()).Finalize(salt_sha512.data());
        sha512.Reset().Write(salt_sha512.data(), salt_sha512.size()).Finalize(salt_sha512.data());

        // Round 1: t=2, m=4096 KiB, p=2; salt = SHA-512²(header)
        uint256 hash;
        {
            argon2_context ctx;
            ctx.out     = hash.begin();
            ctx.outlen  = ARGON2ID_HASHLEN;
            ctx.pwd     = hdr.data();
            ctx.pwdlen  = HEADER_LEN;
            ctx.salt    = salt_sha512.data();
            ctx.saltlen = static_cast<uint32_t>(salt_sha512.size());
            ctx.secret  = nullptr; ctx.secretlen = 0;
            ctx.ad      = nullptr; ctx.adlen     = 0;
            ctx.allocate_cbk = nullptr;
            ctx.free_cbk     = nullptr;
            ctx.flags   = ARGON2_DEFAULT_FLAGS;
            ctx.t_cost  = ARGON2ID_T;
            ctx.m_cost  = ARGON2ID_M1;
            ctx.lanes   = ARGON2ID_P;
            ctx.threads = ARGON2ID_P;
            ctx.version = ARGON2_VERSION_NUMBER;
            const int rc = argon2_ctx(&ctx, Argon2_id);
            assert(rc == ARGON2_OK);
        }

        // Round 2: t=2, m=32768 KiB, p=2; salt = output of round 1
        uint256 hash2;
        {
            argon2_context ctx;
            ctx.out     = hash2.begin();
            ctx.outlen  = ARGON2ID_HASHLEN;
            ctx.pwd     = hdr.data();
            ctx.pwdlen  = HEADER_LEN;
            ctx.salt    = hash.begin();
            ctx.saltlen = 32;
            ctx.secret  = nullptr; ctx.secretlen = 0;
            ctx.ad      = nullptr; ctx.adlen     = 0;
            ctx.allocate_cbk = nullptr;
            ctx.free_cbk     = nullptr;
            ctx.flags   = ARGON2_DEFAULT_FLAGS;
            ctx.t_cost  = ARGON2ID_T;
            ctx.m_cost  = ARGON2ID_M2;
            ctx.lanes   = ARGON2ID_P;
            ctx.threads = ARGON2ID_P;
            ctx.version = ARGON2_VERSION_NUMBER;
            const int rc = argon2_ctx(&ctx, Argon2_id);
            assert(rc == ARGON2_OK);
        }
    });
}

/*
 * One benchmark variant per available ISA, mirroring SHA256_STANDARD /
 * SHA256_SSE4 / SHA256_AVX2 in crypto_hash.cpp.
 * Argon2AutoDetect(impl) selects the implementation exactly as
 * SHA256AutoDetect(impl) does - no #ifdef needed in the bench body.
 * If the requested ISA is unavailable, Argon2AutoDetect silently
 * falls back (to reference on non-x86).
 *
 * ISA variants are compiled in only when the build system detected the
 * required compiler support (HAVE_ARGON2_SSE2 / SSSE3 / AVX2 / AVX512 /
 * NEON in introspection.cmake) and forwarded the matching
 * ENABLE_ARGON2_* definition to this target.  Variants whose
 * ENABLE_ARGON2_* macro is absent are excluded entirely so that
 * bench_dpowcoin never registers a benchmark it cannot meaningfully run.
 */
static void Argon2id_STANDARD(benchmark::Bench& bench)
{
    bench.name(strprintf("%s using the '%s' Argon2id implementation",
        __func__, Argon2AutoDetect(argon2_implementation::STANDARD)));
    RunArgon2idHash(bench);
    Argon2AutoDetect();
}
BENCHMARK(Argon2id_STANDARD);

#if defined(ENABLE_ARGON2_SSE2)
static void Argon2id_SSE2(benchmark::Bench& bench)
{
    bench.name(strprintf("%s using the '%s' Argon2id implementation",
        __func__, Argon2AutoDetect(argon2_implementation::USE_SSE2)));
    RunArgon2idHash(bench);
    Argon2AutoDetect();
}
BENCHMARK(Argon2id_SSE2);
#endif

#if defined(ENABLE_ARGON2_SSSE3)
static void Argon2id_SSSE3(benchmark::Bench& bench)
{
    bench.name(strprintf("%s using the '%s' Argon2id implementation",
        __func__, Argon2AutoDetect(argon2_implementation::USE_SSSE3)));
    RunArgon2idHash(bench);
    Argon2AutoDetect();
}
BENCHMARK(Argon2id_SSSE3);
#endif

#if defined(ENABLE_ARGON2_AVX2)
static void Argon2id_AVX2(benchmark::Bench& bench)
{
    bench.name(strprintf("%s using the '%s' Argon2id implementation",
        __func__, Argon2AutoDetect(argon2_implementation::USE_AVX2)));
    RunArgon2idHash(bench);
    Argon2AutoDetect();
}
BENCHMARK(Argon2id_AVX2);
#endif

#if defined(ENABLE_ARGON2_AVX512)
static void Argon2id_AVX512(benchmark::Bench& bench)
{
    bench.name(strprintf("%s using the '%s' Argon2id implementation",
        __func__, Argon2AutoDetect(argon2_implementation::USE_AVX512)));
    RunArgon2idHash(bench);
    Argon2AutoDetect();
}
BENCHMARK(Argon2id_AVX512);
#endif

/* NEON: AArch64 / ARMv7+NEON - compiled in only on non-x86 builds where
 * introspection.cmake sets HAVE_ARGON2_NEON.  Mutually exclusive with the
 * x86 ISA variants above because HAVE_GETCPUID (cpuid.h) is absent on ARM,
 * so the SSE2/AVX paths are never compiled there anyway. */
#if defined(ENABLE_ARGON2_NEON)
static void Argon2id_NEON(benchmark::Bench& bench)
{
    bench.name(strprintf("%s using the '%s' Argon2id implementation",
        __func__, Argon2AutoDetect(argon2_implementation::USE_NEON)));
    RunArgon2idHash(bench);
    Argon2AutoDetect();
}
BENCHMARK(Argon2id_NEON);
#endif
