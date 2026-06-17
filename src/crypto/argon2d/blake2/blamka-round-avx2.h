/*
 * Argon2 reference source code package - reference C implementations
 *
 * Copyright 2015
 * Daniel Dinu, Dmitry Khovratovich, Jean-Philippe Aumasson, and Samuel Neves
 *
 * You may use this work under the terms of a Creative Commons CC0 1.0
 * License/Waiver or the Apache Public License 2.0, at your option. The terms of
 * these licenses can be found at:
 *
 * - CC0 1.0 Universal : https://creativecommons.org/publicdomain/zero/1.0
 * - Apache 2.0        : https://www.apache.org/licenses/LICENSE-2.0
 *
 * You should have received a copy of both of these licenses along with this
 * software. If not, they may be obtained at the above URLs.
 */

/*
 * AVX2 Blake2b round primitives for Argon2.
 *
 * This header is ISA-specific: __m256i only, no #if __AVX2__ dispatch.
 * Must be included from a translation unit compiled with -mavx2.
 *
 * Future migration note: when the project switches from "..." to <> includes,
 * this file will be included as <crypto/argon2d/blake2/blamka-round-avx2.h>.
 */

#ifndef BITCOIN_CRYPTO_ARGON2D_BLAKE2_BLAMKA_ROUND_AVX2_H
#define BITCOIN_CRYPTO_ARGON2D_BLAKE2_BLAMKA_ROUND_AVX2_H

#include <immintrin.h>          /* AVX2 — compiler defines __AVX2__ via -mavx2 */

/* -------------------------------------------------------------------------
 * Rotation helpers — all expressed with AVX2 shuffle/permute intrinsics.
 * ------------------------------------------------------------------------- */
#define BLAMKA_AVX2_rotr32(x)                                                  \
    _mm256_shuffle_epi32((x), _MM_SHUFFLE(2, 3, 0, 1))
#define BLAMKA_AVX2_rotr24(x)                                                  \
    _mm256_shuffle_epi8((x), _mm256_setr_epi8(                                 \
        3, 4, 5, 6, 7, 0, 1, 2, 11, 12, 13, 14, 15, 8, 9, 10,                 \
        3, 4, 5, 6, 7, 0, 1, 2, 11, 12, 13, 14, 15, 8, 9, 10))
#define BLAMKA_AVX2_rotr16(x)                                                  \
    _mm256_shuffle_epi8((x), _mm256_setr_epi8(                                 \
        2, 3, 4, 5, 6, 7, 0, 1, 10, 11, 12, 13, 14, 15, 8, 9,                 \
        2, 3, 4, 5, 6, 7, 0, 1, 10, 11, 12, 13, 14, 15, 8, 9))
#define BLAMKA_AVX2_rotr63(x)                                                  \
    _mm256_xor_si256(_mm256_srli_epi64((x), 63), _mm256_add_epi64((x), (x)))

/* -------------------------------------------------------------------------
 * G1_AVX2 / G2_AVX2 — first and second half of one Blake2b G application,
 * each operating on two parallel __m256i columns at once.
 * ------------------------------------------------------------------------- */
#define G1_AVX2(A0, A1, B0, B1, C0, C1, D0, D1)                               \
    do {                                                                       \
        __m256i _ml;                                                           \
        _ml = _mm256_mul_epu32(A0, B0);                                        \
        _ml = _mm256_add_epi64(_ml, _ml);                                      \
        A0  = _mm256_add_epi64(A0, _mm256_add_epi64(B0, _ml));                 \
        D0  = _mm256_xor_si256(D0, A0);                                        \
        D0  = BLAMKA_AVX2_rotr32(D0);                                          \
        _ml = _mm256_mul_epu32(C0, D0);                                        \
        _ml = _mm256_add_epi64(_ml, _ml);                                      \
        C0  = _mm256_add_epi64(C0, _mm256_add_epi64(D0, _ml));                 \
        B0  = _mm256_xor_si256(B0, C0);                                        \
        B0  = BLAMKA_AVX2_rotr24(B0);                                          \
                                                                               \
        _ml = _mm256_mul_epu32(A1, B1);                                        \
        _ml = _mm256_add_epi64(_ml, _ml);                                      \
        A1  = _mm256_add_epi64(A1, _mm256_add_epi64(B1, _ml));                 \
        D1  = _mm256_xor_si256(D1, A1);                                        \
        D1  = BLAMKA_AVX2_rotr32(D1);                                          \
        _ml = _mm256_mul_epu32(C1, D1);                                        \
        _ml = _mm256_add_epi64(_ml, _ml);                                      \
        C1  = _mm256_add_epi64(C1, _mm256_add_epi64(D1, _ml));                 \
        B1  = _mm256_xor_si256(B1, C1);                                        \
        B1  = BLAMKA_AVX2_rotr24(B1);                                          \
    } while ((void)0, 0)

#define G2_AVX2(A0, A1, B0, B1, C0, C1, D0, D1)                               \
    do {                                                                       \
        __m256i _ml;                                                           \
        _ml = _mm256_mul_epu32(A0, B0);                                        \
        _ml = _mm256_add_epi64(_ml, _ml);                                      \
        A0  = _mm256_add_epi64(A0, _mm256_add_epi64(B0, _ml));                 \
        D0  = _mm256_xor_si256(D0, A0);                                        \
        D0  = BLAMKA_AVX2_rotr16(D0);                                          \
        _ml = _mm256_mul_epu32(C0, D0);                                        \
        _ml = _mm256_add_epi64(_ml, _ml);                                      \
        C0  = _mm256_add_epi64(C0, _mm256_add_epi64(D0, _ml));                 \
        B0  = _mm256_xor_si256(B0, C0);                                        \
        B0  = BLAMKA_AVX2_rotr63(B0);                                          \
                                                                               \
        _ml = _mm256_mul_epu32(A1, B1);                                        \
        _ml = _mm256_add_epi64(_ml, _ml);                                      \
        A1  = _mm256_add_epi64(A1, _mm256_add_epi64(B1, _ml));                 \
        D1  = _mm256_xor_si256(D1, A1);                                        \
        D1  = BLAMKA_AVX2_rotr16(D1);                                          \
        _ml = _mm256_mul_epu32(C1, D1);                                        \
        _ml = _mm256_add_epi64(_ml, _ml);                                      \
        C1  = _mm256_add_epi64(C1, _mm256_add_epi64(D1, _ml));                 \
        B1  = _mm256_xor_si256(B1, C1);                                        \
        B1  = BLAMKA_AVX2_rotr63(B1);                                          \
    } while ((void)0, 0)

/* -------------------------------------------------------------------------
 * DIAGONALIZE_1 / UNDIAGONALIZE_1 — column-pass permutation (__m256i pair).
 * DIAGONALIZE_2 / UNDIAGONALIZE_2 — row-pass permutation (__m256i pair).
 * ------------------------------------------------------------------------- */
#define DIAGONALIZE_1_AVX2(A0, B0, C0, D0, A1, B1, C1, D1)                    \
    do {                                                                       \
        B0 = _mm256_permute4x64_epi64(B0, _MM_SHUFFLE(0, 3, 2, 1));            \
        C0 = _mm256_permute4x64_epi64(C0, _MM_SHUFFLE(1, 0, 3, 2));            \
        D0 = _mm256_permute4x64_epi64(D0, _MM_SHUFFLE(2, 1, 0, 3));            \
        B1 = _mm256_permute4x64_epi64(B1, _MM_SHUFFLE(0, 3, 2, 1));            \
        C1 = _mm256_permute4x64_epi64(C1, _MM_SHUFFLE(1, 0, 3, 2));            \
        D1 = _mm256_permute4x64_epi64(D1, _MM_SHUFFLE(2, 1, 0, 3));            \
    } while ((void)0, 0)

#define UNDIAGONALIZE_1_AVX2(A0, B0, C0, D0, A1, B1, C1, D1)                  \
    do {                                                                       \
        B0 = _mm256_permute4x64_epi64(B0, _MM_SHUFFLE(2, 1, 0, 3));            \
        C0 = _mm256_permute4x64_epi64(C0, _MM_SHUFFLE(1, 0, 3, 2));            \
        D0 = _mm256_permute4x64_epi64(D0, _MM_SHUFFLE(0, 3, 2, 1));            \
        B1 = _mm256_permute4x64_epi64(B1, _MM_SHUFFLE(2, 1, 0, 3));            \
        C1 = _mm256_permute4x64_epi64(C1, _MM_SHUFFLE(1, 0, 3, 2));            \
        D1 = _mm256_permute4x64_epi64(D1, _MM_SHUFFLE(0, 3, 2, 1));            \
    } while ((void)0, 0)

#define DIAGONALIZE_2_AVX2(A0, A1, B0, B1, C0, C1, D0, D1)                    \
    do {                                                                       \
        __m256i _t1 = _mm256_blend_epi32(B0, B1, 0xCC);                        \
        __m256i _t2 = _mm256_blend_epi32(B0, B1, 0x33);                        \
        B1 = _mm256_permute4x64_epi64(_t1, _MM_SHUFFLE(2, 3, 0, 1));           \
        B0 = _mm256_permute4x64_epi64(_t2, _MM_SHUFFLE(2, 3, 0, 1));           \
        _t1 = C0;  C0 = C1;  C1 = _t1;                                        \
        _t1 = _mm256_blend_epi32(D0, D1, 0xCC);                                \
        _t2 = _mm256_blend_epi32(D0, D1, 0x33);                                \
        D0 = _mm256_permute4x64_epi64(_t1, _MM_SHUFFLE(2, 3, 0, 1));           \
        D1 = _mm256_permute4x64_epi64(_t2, _MM_SHUFFLE(2, 3, 0, 1));           \
    } while ((void)0, 0)

#define UNDIAGONALIZE_2_AVX2(A0, A1, B0, B1, C0, C1, D0, D1)                  \
    do {                                                                       \
        __m256i _t1 = _mm256_blend_epi32(B0, B1, 0xCC);                        \
        __m256i _t2 = _mm256_blend_epi32(B0, B1, 0x33);                        \
        B0 = _mm256_permute4x64_epi64(_t1, _MM_SHUFFLE(2, 3, 0, 1));           \
        B1 = _mm256_permute4x64_epi64(_t2, _MM_SHUFFLE(2, 3, 0, 1));           \
        _t1 = C0;  C0 = C1;  C1 = _t1;                                        \
        _t1 = _mm256_blend_epi32(D0, D1, 0x33);                                \
        _t2 = _mm256_blend_epi32(D0, D1, 0xCC);                                \
        D0 = _mm256_permute4x64_epi64(_t1, _MM_SHUFFLE(2, 3, 0, 1));           \
        D1 = _mm256_permute4x64_epi64(_t2, _MM_SHUFFLE(2, 3, 0, 1));           \
    } while ((void)0, 0)

/* -------------------------------------------------------------------------
 * BLAKE2_ROUND_1_AVX2 — column pass (8 __m256i state elements, row-major).
 * BLAKE2_ROUND_2_AVX2 — row    pass (8 __m256i state elements, column-major).
 *
 * Each macro expands to a full Blake2b round (two G + diagonalize halves).
 * opt_avx2.cpp calls ROUND_1 for the 4 column iterations and ROUND_2 for
 * the 4 row iterations of fill_block().
 * ------------------------------------------------------------------------- */
#define BLAKE2_ROUND_1_AVX2(A0, A1, B0, B1, C0, C1, D0, D1)                   \
    do {                                                                       \
        G1_AVX2(A0, A1, B0, B1, C0, C1, D0, D1);                               \
        G2_AVX2(A0, A1, B0, B1, C0, C1, D0, D1);                               \
        DIAGONALIZE_1_AVX2(A0, B0, C0, D0, A1, B1, C1, D1);                    \
        G1_AVX2(A0, A1, B0, B1, C0, C1, D0, D1);                               \
        G2_AVX2(A0, A1, B0, B1, C0, C1, D0, D1);                               \
        UNDIAGONALIZE_1_AVX2(A0, B0, C0, D0, A1, B1, C1, D1);                  \
    } while ((void)0, 0)

#define BLAKE2_ROUND_2_AVX2(A0, A1, B0, B1, C0, C1, D0, D1)                   \
    do {                                                                       \
        G1_AVX2(A0, A1, B0, B1, C0, C1, D0, D1);                               \
        G2_AVX2(A0, A1, B0, B1, C0, C1, D0, D1);                               \
        DIAGONALIZE_2_AVX2(A0, A1, B0, B1, C0, C1, D0, D1);                    \
        G1_AVX2(A0, A1, B0, B1, C0, C1, D0, D1);                               \
        G2_AVX2(A0, A1, B0, B1, C0, C1, D0, D1);                               \
        UNDIAGONALIZE_2_AVX2(A0, A1, B0, B1, C0, C1, D0, D1);                  \
    } while ((void)0, 0)

#endif // BITCOIN_CRYPTO_ARGON2D_BLAKE2_BLAMKA_ROUND_AVX2_H
