/*
 * Argon2 reference source code package - reference C implementations
 *
 * Copyright 2015
 * Daniel Dinu, Dmitry Khovratovich, Jean-Philippe Aumasson, and Samuel Neves
 *
 * You may use this work under the terms of a Creative Commons CC0 1.0
 * License/Waiver or the Apache Public License 2.0, at your option.
 */

/*
 * AVX-512F Blake2b round primitives for Argon2.
 * ISA-specific: __m512i only, no #if __AVX512F__ dispatch.
 * Must be compiled with -mavx512f.
 *
 * Future <>-migration: <crypto/argon2d/blake2/blamka-round-avx512.h>
 */

#ifndef BITCOIN_CRYPTO_ARGON2D_BLAKE2_BLAMKA_ROUND_AVX512_H
#define BITCOIN_CRYPTO_ARGON2D_BLAKE2_BLAMKA_ROUND_AVX512_H

#include <immintrin.h>

#define BLAMKA_AVX512_ror64(x, n) _mm512_ror_epi64((x), (n))

static inline __m512i muladd_avx512(__m512i x, __m512i y)
{
    __m512i z = _mm512_mul_epu32(x, y);
    return _mm512_add_epi64(_mm512_add_epi64(x, y), _mm512_add_epi64(z, z));
}

#define G1_AVX512(A0, B0, C0, D0, A1, B1, C1, D1)                             \
    do {                                                                       \
        A0 = muladd_avx512(A0, B0);  A1 = muladd_avx512(A1, B1);              \
        D0 = _mm512_xor_si512(D0, A0); D1 = _mm512_xor_si512(D1, A1);         \
        D0 = BLAMKA_AVX512_ror64(D0, 32); D1 = BLAMKA_AVX512_ror64(D1, 32);   \
        C0 = muladd_avx512(C0, D0);  C1 = muladd_avx512(C1, D1);              \
        B0 = _mm512_xor_si512(B0, C0); B1 = _mm512_xor_si512(B1, C1);         \
        B0 = BLAMKA_AVX512_ror64(B0, 24); B1 = BLAMKA_AVX512_ror64(B1, 24);   \
    } while ((void)0, 0)

#define G2_AVX512(A0, B0, C0, D0, A1, B1, C1, D1)                             \
    do {                                                                       \
        A0 = muladd_avx512(A0, B0);  A1 = muladd_avx512(A1, B1);              \
        D0 = _mm512_xor_si512(D0, A0); D1 = _mm512_xor_si512(D1, A1);         \
        D0 = BLAMKA_AVX512_ror64(D0, 16); D1 = BLAMKA_AVX512_ror64(D1, 16);   \
        C0 = muladd_avx512(C0, D0);  C1 = muladd_avx512(C1, D1);              \
        B0 = _mm512_xor_si512(B0, C0); B1 = _mm512_xor_si512(B1, C1);         \
        B0 = BLAMKA_AVX512_ror64(B0, 63); B1 = BLAMKA_AVX512_ror64(B1, 63);   \
    } while ((void)0, 0)

#define DIAGONALIZE_AVX512(A0, B0, C0, D0, A1, B1, C1, D1)                    \
    do {                                                                       \
        B0 = _mm512_permutex_epi64(B0, _MM_SHUFFLE(0, 3, 2, 1));               \
        B1 = _mm512_permutex_epi64(B1, _MM_SHUFFLE(0, 3, 2, 1));               \
        C0 = _mm512_permutex_epi64(C0, _MM_SHUFFLE(1, 0, 3, 2));               \
        C1 = _mm512_permutex_epi64(C1, _MM_SHUFFLE(1, 0, 3, 2));               \
        D0 = _mm512_permutex_epi64(D0, _MM_SHUFFLE(2, 1, 0, 3));               \
        D1 = _mm512_permutex_epi64(D1, _MM_SHUFFLE(2, 1, 0, 3));               \
    } while ((void)0, 0)

#define UNDIAGONALIZE_AVX512(A0, B0, C0, D0, A1, B1, C1, D1)                  \
    do {                                                                       \
        B0 = _mm512_permutex_epi64(B0, _MM_SHUFFLE(2, 1, 0, 3));               \
        B1 = _mm512_permutex_epi64(B1, _MM_SHUFFLE(2, 1, 0, 3));               \
        C0 = _mm512_permutex_epi64(C0, _MM_SHUFFLE(1, 0, 3, 2));               \
        C1 = _mm512_permutex_epi64(C1, _MM_SHUFFLE(1, 0, 3, 2));               \
        D0 = _mm512_permutex_epi64(D0, _MM_SHUFFLE(0, 3, 2, 1));               \
        D1 = _mm512_permutex_epi64(D1, _MM_SHUFFLE(0, 3, 2, 1));               \
    } while ((void)0, 0)

#define BLAKE2_ROUND_AVX512(A0, B0, C0, D0, A1, B1, C1, D1)                   \
    do {                                                                       \
        G1_AVX512(A0, B0, C0, D0, A1, B1, C1, D1);                             \
        G2_AVX512(A0, B0, C0, D0, A1, B1, C1, D1);                             \
        DIAGONALIZE_AVX512(A0, B0, C0, D0, A1, B1, C1, D1);                    \
        G1_AVX512(A0, B0, C0, D0, A1, B1, C1, D1);                             \
        G2_AVX512(A0, B0, C0, D0, A1, B1, C1, D1);                             \
        UNDIAGONALIZE_AVX512(A0, B0, C0, D0, A1, B1, C1, D1);                  \
    } while ((void)0, 0)

/* _mm512_setr_epi64 args are compile-time constants — compiler folds to immediate load */
#define SWAP_HALVES_AVX512(A0, A1)                                             \
    do {                                                                       \
        __m512i _t0 = _mm512_shuffle_i64x2((A0), (A1), _MM_SHUFFLE(1, 0, 1, 0)); \
        __m512i _t1 = _mm512_shuffle_i64x2((A0), (A1), _MM_SHUFFLE(3, 2, 3, 2)); \
        (A0) = _t0;  (A1) = _t1;                                               \
    } while ((void)0, 0)

#define SWAP_QUARTERS_AVX512(A0, A1)                                           \
    do {                                                                       \
        __m512i _idx = _mm512_setr_epi64(0, 1, 4, 5, 2, 3, 6, 7);             \
        SWAP_HALVES_AVX512(A0, A1);                                            \
        (A0) = _mm512_permutexvar_epi64(_idx, (A0));                           \
        (A1) = _mm512_permutexvar_epi64(_idx, (A1));                           \
    } while ((void)0, 0)

#define UNSWAP_QUARTERS_AVX512(A0, A1)                                         \
    do {                                                                       \
        __m512i _idx = _mm512_setr_epi64(0, 1, 4, 5, 2, 3, 6, 7);             \
        (A0) = _mm512_permutexvar_epi64(_idx, (A0));                           \
        (A1) = _mm512_permutexvar_epi64(_idx, (A1));                           \
        SWAP_HALVES_AVX512(A0, A1);                                            \
    } while ((void)0, 0)

#define BLAKE2_ROUND_1_AVX512(A0, C0, B0, D0, A1, C1, B1, D1)                 \
    do {                                                                       \
        SWAP_HALVES_AVX512(A0, B0); SWAP_HALVES_AVX512(C0, D0);                \
        SWAP_HALVES_AVX512(A1, B1); SWAP_HALVES_AVX512(C1, D1);                \
        BLAKE2_ROUND_AVX512(A0, B0, C0, D0, A1, B1, C1, D1);                   \
        SWAP_HALVES_AVX512(A0, B0); SWAP_HALVES_AVX512(C0, D0);                \
        SWAP_HALVES_AVX512(A1, B1); SWAP_HALVES_AVX512(C1, D1);                \
    } while ((void)0, 0)

#define BLAKE2_ROUND_2_AVX512(A0, A1, B0, B1, C0, C1, D0, D1)                 \
    do {                                                                       \
        SWAP_QUARTERS_AVX512(A0, A1); SWAP_QUARTERS_AVX512(B0, B1);            \
        SWAP_QUARTERS_AVX512(C0, C1); SWAP_QUARTERS_AVX512(D0, D1);            \
        BLAKE2_ROUND_AVX512(A0, B0, C0, D0, A1, B1, C1, D1);                   \
        UNSWAP_QUARTERS_AVX512(A0, A1); UNSWAP_QUARTERS_AVX512(B0, B1);        \
        UNSWAP_QUARTERS_AVX512(C0, C1); UNSWAP_QUARTERS_AVX512(D0, D1);        \
    } while ((void)0, 0)

#endif // BITCOIN_CRYPTO_ARGON2D_BLAKE2_BLAMKA_ROUND_AVX512_H
