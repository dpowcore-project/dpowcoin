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
 * SSE2 Blake2b round primitives for Argon2.
 *
 * This header is intentionally ISA-specific: it contains ONLY the SSE2
 * compiled with -mavx2 or -mavx512f, because those macros are redundantly
 * re-defined here under unique names (BLAKE2_ROUND_SSE2 etc.) and the old
 * monolithic blamka-round-opt.h's #if !defined(__AVX2__) dispatch is gone.
 *
 * Future migration note: when the project switches from "..." to <> includes,
 * this file will be included as <crypto/argon2d/blake2/blamka-round-sse2.h>.
 */

#ifndef BITCOIN_CRYPTO_ARGON2D_BLAKE2_BLAMKA_ROUND_SSE2_H
#define BITCOIN_CRYPTO_ARGON2D_BLAKE2_BLAMKA_ROUND_SSE2_H

#include <emmintrin.h>          /* SSE2 — always available when this file is used */

/* blake2-impl.h provides BLAKE2_INLINE, load64/store64 etc.
 * Future <>-migration: <crypto/argon2d/blake2/blake2-impl.h>            */
#include <crypto/argon2d/blake2/blake2-impl.h>

#define BLAMKA_SSE2_mm_roti_epi64(r, c)                                      \
    _mm_xor_si128(_mm_srli_epi64((r), -(c)), _mm_slli_epi64((r), 64 - (-(c))))

/* -------------------------------------------------------------------------
 * fBlaMka — multiply-add mixing function (Argon2 variant of Blake2b G).
 * ------------------------------------------------------------------------- */
static BLAKE2_INLINE __m128i fBlaMka_sse2(__m128i x, __m128i y)
{
    const __m128i z = _mm_mul_epu32(x, y);
    return _mm_add_epi64(_mm_add_epi64(x, y), _mm_add_epi64(z, z));
}

/* -------------------------------------------------------------------------
 * G1_SSE2 / G2_SSE2 — first and second half of one Blake2b G application,
 * operating on two parallel __m128i columns at once.
 * ------------------------------------------------------------------------- */
#define G1_SSE2(A0, B0, C0, D0, A1, B1, C1, D1)                               \
    do {                                                                       \
        A0 = fBlaMka_sse2(A0, B0);                                             \
        A1 = fBlaMka_sse2(A1, B1);                                             \
                                                                               \
        D0 = _mm_xor_si128(D0, A0);                                            \
        D1 = _mm_xor_si128(D1, A1);                                            \
                                                                               \
        D0 = BLAMKA_SSE2_mm_roti_epi64(D0, -32);                               \
        D1 = BLAMKA_SSE2_mm_roti_epi64(D1, -32);                               \
                                                                               \
        C0 = fBlaMka_sse2(C0, D0);                                             \
        C1 = fBlaMka_sse2(C1, D1);                                             \
                                                                               \
        B0 = _mm_xor_si128(B0, C0);                                            \
        B1 = _mm_xor_si128(B1, C1);                                            \
                                                                               \
        B0 = BLAMKA_SSE2_mm_roti_epi64(B0, -24);                               \
        B1 = BLAMKA_SSE2_mm_roti_epi64(B1, -24);                               \
    } while ((void)0, 0)

#define G2_SSE2(A0, B0, C0, D0, A1, B1, C1, D1)                               \
    do {                                                                       \
        A0 = fBlaMka_sse2(A0, B0);                                             \
        A1 = fBlaMka_sse2(A1, B1);                                             \
                                                                               \
        D0 = _mm_xor_si128(D0, A0);                                            \
        D1 = _mm_xor_si128(D1, A1);                                            \
                                                                               \
        D0 = BLAMKA_SSE2_mm_roti_epi64(D0, -16);                               \
        D1 = BLAMKA_SSE2_mm_roti_epi64(D1, -16);                               \
                                                                               \
        C0 = fBlaMka_sse2(C0, D0);                                             \
        C1 = fBlaMka_sse2(C1, D1);                                             \
                                                                               \
        B0 = _mm_xor_si128(B0, C0);                                            \
        B1 = _mm_xor_si128(B1, C1);                                            \
                                                                               \
        B0 = BLAMKA_SSE2_mm_roti_epi64(B0, -63);                               \
        B1 = BLAMKA_SSE2_mm_roti_epi64(B1, -63);                               \
    } while ((void)0, 0)

/* -------------------------------------------------------------------------
 * DIAGONALIZE / UNDIAGONALIZE — permute 8 __m128i values into diagonal form.
 * ------------------------------------------------------------------------- */
#define DIAGONALIZE_SSE2(A0, B0, C0, D0, A1, B1, C1, D1)                    \
    do {                                                                       \
        __m128i t0 = D0;                                                       \
        __m128i t1 = B0;                                                       \
        D0 = C0;  C0 = C1;  C1 = D0;                                           \
        D0 = _mm_unpackhi_epi64(D1, _mm_unpacklo_epi64(t0, t0));               \
        D1 = _mm_unpackhi_epi64(t0, _mm_unpacklo_epi64(D1, D1));               \
        B0 = _mm_unpackhi_epi64(B0, _mm_unpacklo_epi64(B1, B1));               \
        B1 = _mm_unpackhi_epi64(B1, _mm_unpacklo_epi64(t1, t1));               \
    } while ((void)0, 0)

#define UNDIAGONALIZE_SSE2(A0, B0, C0, D0, A1, B1, C1, D1)                  \
    do {                                                                       \
        __m128i t0, t1;                                                        \
        t0 = C0;  C0 = C1;  C1 = t0;                                           \
        t0 = B0;  t1 = D0;                                                     \
        B0 = _mm_unpackhi_epi64(B1, _mm_unpacklo_epi64(B0, B0));               \
        B1 = _mm_unpackhi_epi64(t0, _mm_unpacklo_epi64(B1, B1));               \
        D0 = _mm_unpackhi_epi64(D0, _mm_unpacklo_epi64(D1, D1));               \
        D1 = _mm_unpackhi_epi64(D1, _mm_unpacklo_epi64(t1, t1));               \
    } while ((void)0, 0)

/* -------------------------------------------------------------------------
 * BLAKE2_ROUND_SSE2 — full Blake2b round on 8 __m128i registers.
 *
 * Applies G twice (column pass + diagonal pass) to (A0,A1,B0,B1,C0,C1,D0,D1).
 * Caller passes the 8 state elements for one 8×128-bit slice; the macro
 * is invoked 8× over columns and 8× over rows in fill_block().
 * ------------------------------------------------------------------------- */
#define BLAKE2_ROUND_SSE2(A0, A1, B0, B1, C0, C1, D0, D1)                     \
    do {                                                                       \
        G1_SSE2(A0, B0, C0, D0, A1, B1, C1, D1);                               \
        G2_SSE2(A0, B0, C0, D0, A1, B1, C1, D1);                               \
                                                                               \
        DIAGONALIZE_SSE2(A0, B0, C0, D0, A1, B1, C1, D1);                      \
                                                                               \
        G1_SSE2(A0, B0, C0, D0, A1, B1, C1, D1);                               \
        G2_SSE2(A0, B0, C0, D0, A1, B1, C1, D1);                               \
                                                                               \
        UNDIAGONALIZE_SSE2(A0, B0, C0, D0, A1, B1, C1, D1);                    \
    } while ((void)0, 0)

#endif // BITCOIN_CRYPTO_ARGON2D_BLAKE2_BLAMKA_ROUND_SSE2_H
