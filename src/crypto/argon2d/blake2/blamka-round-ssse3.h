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
 * SSSE3 Blake2b round primitives for Argon2.
 *
 * Self-contained: includes <tmmintrin.h> directly and defines all rotation
 * and diagonalize helpers unconditionally using SSSE3 shuffle instructions.
 * Must be compiled with -mssse3 (i.e. from argon2_opt_ssse3.cpp only).
 *
 * WHY THIS IS A SEPARATE FILE (not a conditional block in blamka-round-sse2.h):
 *
 * blamka-round-sse2.h contains:
 *   #if defined(__SSSE3__)
 *     ...shuffle-based rotations...
 *   #else
 *     ...shift+or fallback...
 *   #endif
 *
 * This pattern is dangerous. Any build host with a modern CPU (post-2008, i.e.
 * virtually all of them) has SSSE3 and will define __SSSE3__ even when the
 * compilation unit is compiled with -msse2, if the base -march= flag is broad
 * enough (e.g. -march=native, -march=x86-64-v2, or many distro defaults).
 * The result: argon2_opt_sse2.cpp silently becomes identical to SSSE3, the
 * separate SSSE3 tier provides zero benefit, and the distinction in the
 * dispatcher becomes meaningless.
 *
 * This is exactly the same problem that previously caused argon2_opt_sse2.cpp
 * and the old monolithic blamka-round-opt.h to collide when the base -march
 * defined __AVX2__.
 *
 * Solution: this file defines SSSE3 paths unconditionally. blamka-round-sse2.h
 * is not included here. The SSE2 translation unit sees only pure-SSE2 code
 * regardless of the build host's capabilities; the SSSE3 unit always gets
 * the faster shuffle paths. Clean, unambiguous.
 *
 * Future <>-migration: <crypto/argon2d/blake2/blamka-round-ssse3.h>
 */

#ifndef BITCOIN_CRYPTO_ARGON2D_BLAKE2_BLAMKA_ROUND_SSSE3_H
#define BITCOIN_CRYPTO_ARGON2D_BLAKE2_BLAMKA_ROUND_SSSE3_H

#include <emmintrin.h>   /* SSE2 baseline — _mm_xor_si128, _mm_add_epi64, etc. */
#include <tmmintrin.h>   /* SSSE3 — _mm_shuffle_epi8, _mm_alignr_epi8         */

#include <crypto/argon2d/blake2/blake2-impl.h>   /* BLAKE2_INLINE */

/* -------------------------------------------------------------------------
 * Rotation constants.
 * Each _mm_setr_epi8 computes a compile-time constant; the compiler folds
 * these to a single memory load or register fill.
 * ------------------------------------------------------------------------- */
#define BLAMKA_SSSE3_r16                                                       \
    (_mm_setr_epi8(2, 3, 4, 5, 6, 7, 0, 1, 10, 11, 12, 13, 14, 15, 8, 9))
#define BLAMKA_SSSE3_r24                                                       \
    (_mm_setr_epi8(3, 4, 5, 6, 7, 0, 1, 2, 11, 12, 13, 14, 15, 8, 9, 10))

/* -------------------------------------------------------------------------
 * Rotation helpers — all SSSE3-native.
 *
 * rotr32: _mm_shuffle_epi32  (same as SSE2, already optimal)
 * rotr24: _mm_shuffle_epi8 with BLAMKA_SSSE3_r24
 * rotr16: _mm_shuffle_epi8 with BLAMKA_SSSE3_r16
 * rotr63: shift+xor (no shuffle equivalent; same cost as SSE2)
 * ------------------------------------------------------------------------- */
#define BLAMKA_SSSE3_mm_roti_epi64(x, c)                                       \
    (-(c) == 32                                                                \
        ? _mm_shuffle_epi32((x), _MM_SHUFFLE(2, 3, 0, 1))                      \
        : (-(c) == 24                                                          \
              ? _mm_shuffle_epi8((x), BLAMKA_SSSE3_r24)                        \
              : (-(c) == 16                                                    \
                    ? _mm_shuffle_epi8((x), BLAMKA_SSSE3_r16)                  \
                    : _mm_xor_si128(_mm_srli_epi64((x), -(c)),                 \
                                    _mm_add_epi64((x), (x))))))

/* -------------------------------------------------------------------------
 * fBlaMka — Argon2 multiply-add mixing function (SSSE3 version).
 * _mm_mul_epu32 is SSE2; no SSSE3-specific instruction available here.
 * ------------------------------------------------------------------------- */
static BLAKE2_INLINE __m128i fBlaMka_ssse3(__m128i x, __m128i y)
{
    const __m128i z = _mm_mul_epu32(x, y);
    return _mm_add_epi64(_mm_add_epi64(x, y), _mm_add_epi64(z, z));
}

/* -------------------------------------------------------------------------
 * G1_SSSE3 / G2_SSSE3 — first and second half of one Blake2b G application,
 * operating on two parallel __m128i columns at once.
 * rotr24 and rotr16 use _mm_shuffle_epi8 (SSSE3) instead of shift+or (SSE2).
 * ------------------------------------------------------------------------- */
#define G1_SSSE3(A0, B0, C0, D0, A1, B1, C1, D1)                              \
    do {                                                                       \
        A0 = fBlaMka_ssse3(A0, B0);                                            \
        A1 = fBlaMka_ssse3(A1, B1);                                            \
                                                                               \
        D0 = _mm_xor_si128(D0, A0);                                            \
        D1 = _mm_xor_si128(D1, A1);                                            \
                                                                               \
        D0 = BLAMKA_SSSE3_mm_roti_epi64(D0, -32);                              \
        D1 = BLAMKA_SSSE3_mm_roti_epi64(D1, -32);                              \
                                                                               \
        C0 = fBlaMka_ssse3(C0, D0);                                            \
        C1 = fBlaMka_ssse3(C1, D1);                                            \
                                                                               \
        B0 = _mm_xor_si128(B0, C0);                                            \
        B1 = _mm_xor_si128(B1, C1);                                            \
                                                                               \
        B0 = BLAMKA_SSSE3_mm_roti_epi64(B0, -24);                              \
        B1 = BLAMKA_SSSE3_mm_roti_epi64(B1, -24);                              \
    } while ((void)0, 0)

#define G2_SSSE3(A0, B0, C0, D0, A1, B1, C1, D1)                              \
    do {                                                                       \
        A0 = fBlaMka_ssse3(A0, B0);                                            \
        A1 = fBlaMka_ssse3(A1, B1);                                            \
                                                                               \
        D0 = _mm_xor_si128(D0, A0);                                            \
        D1 = _mm_xor_si128(D1, A1);                                            \
                                                                               \
        D0 = BLAMKA_SSSE3_mm_roti_epi64(D0, -16);                              \
        D1 = BLAMKA_SSSE3_mm_roti_epi64(D1, -16);                              \
                                                                               \
        C0 = fBlaMka_ssse3(C0, D0);                                            \
        C1 = fBlaMka_ssse3(C1, D1);                                            \
                                                                               \
        B0 = _mm_xor_si128(B0, C0);                                            \
        B1 = _mm_xor_si128(B1, C1);                                            \
                                                                               \
        B0 = BLAMKA_SSSE3_mm_roti_epi64(B0, -63);                              \
        B1 = BLAMKA_SSSE3_mm_roti_epi64(B1, -63);                              \
    } while ((void)0, 0)

/* -------------------------------------------------------------------------
 * DIAGONALIZE_SSSE3 / UNDIAGONALIZE_SSSE3 — permute 8 __m128i values.
 *
 * Uses _mm_alignr_epi8 (SSSE3) which is faster than the SSE2 unpack
 * sequence. This is the other key benefit of SSSE3 beyond the rotations.
 * ------------------------------------------------------------------------- */
#define DIAGONALIZE_SSSE3(A0, B0, C0, D0, A1, B1, C1, D1)                     \
    do {                                                                       \
        __m128i t0 = _mm_alignr_epi8(B1, B0, 8);                               \
        __m128i t1 = _mm_alignr_epi8(B0, B1, 8);                               \
        B0 = t0;  B1 = t1;                                                     \
        t0 = C0;  C0 = C1;  C1 = t0;                                           \
        t0 = _mm_alignr_epi8(D1, D0, 8);                                       \
        t1 = _mm_alignr_epi8(D0, D1, 8);                                       \
        D0 = t1;  D1 = t0;                                                     \
    } while ((void)0, 0)

#define UNDIAGONALIZE_SSSE3(A0, B0, C0, D0, A1, B1, C1, D1)                   \
    do {                                                                       \
        __m128i t0 = _mm_alignr_epi8(B0, B1, 8);                               \
        __m128i t1 = _mm_alignr_epi8(B1, B0, 8);                               \
        B0 = t0;  B1 = t1;                                                     \
        t0 = C0;  C0 = C1;  C1 = t0;                                           \
        t0 = _mm_alignr_epi8(D0, D1, 8);                                       \
        t1 = _mm_alignr_epi8(D1, D0, 8);                                       \
        D0 = t1;  D1 = t0;                                                     \
    } while ((void)0, 0)

/* -------------------------------------------------------------------------
 * BLAKE2_ROUND_SSSE3 — full Blake2b round on 8 __m128i registers.
 *
 * Uses SSSE3 shuffle for all four rotations and _mm_alignr_epi8 for
 * diagonalize. On Sandy Bridge / Ivy Bridge (no AVX2) this is the fastest
 * available path; on Haswell+ the AVX2 path wins.
 * ------------------------------------------------------------------------- */
#define BLAKE2_ROUND_SSSE3(A0, A1, B0, B1, C0, C1, D0, D1)                    \
    do {                                                                       \
        G1_SSSE3(A0, B0, C0, D0, A1, B1, C1, D1);                              \
        G2_SSSE3(A0, B0, C0, D0, A1, B1, C1, D1);                              \
                                                                               \
        DIAGONALIZE_SSSE3(A0, B0, C0, D0, A1, B1, C1, D1);                     \
                                                                               \
        G1_SSSE3(A0, B0, C0, D0, A1, B1, C1, D1);                              \
        G2_SSSE3(A0, B0, C0, D0, A1, B1, C1, D1);                              \
                                                                               \
        UNDIAGONALIZE_SSSE3(A0, B0, C0, D0, A1, B1, C1, D1);                   \
    } while ((void)0, 0)

#endif // BITCOIN_CRYPTO_ARGON2D_BLAKE2_BLAMKA_ROUND_SSSE3_H
