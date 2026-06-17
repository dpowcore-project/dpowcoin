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
 * Portable ARM NEON Blake2b round primitives for Argon2.
 *
 * Targets the common NEON subset available on BOTH:
 *   - AArch64 (ARMv8-A 64-bit) — NEON is mandatory baseline.
 *   - ARMv7 with NEON          — compiled with -mfpu=neon.
 *
 * rotr24 / rotr16:
 *   On AArch64 and Apple Silicon (__aarch64__ / __arm64__):
 *     vqtbl1q_u8 — single-register 128-bit table lookup, one instruction.
 *   On ARMv7+NEON:
 *     portable shift+OR — vqtbl1q_u8 is AArch64-only and would SIGILL on ARMv7.
 *
 * All other primitives (vmovn_u64, vmull_u32, vaddq_u64, veorq_u64,
 * vreinterpretq_u64_u32, vrev64q_u32, vshrq_n_u64, vshlq_n_u64, vorrq_u64,
 * vextq_u8, vld1q_u8, vld1q_u64, vst1q_u64) are available on both targets.
 *
 * Must be included only from a translation unit compiled with NEON support
 * (no extra flags on AArch64; -mfpu=neon on ARMv7).
 */

#ifndef BITCOIN_CRYPTO_ARGON2D_BLAKE2_BLAMKA_ROUND_NEON_H
#define BITCOIN_CRYPTO_ARGON2D_BLAKE2_BLAMKA_ROUND_NEON_H

#include <arm_neon.h>

/* -------------------------------------------------------------------------
 * fBlaMka_neon — modified G mixing function used by Argon2.
 *
 * Computes: x + y + 2*(lo32(x) * lo32(y))
 * ------------------------------------------------------------------------- */
static inline uint64x2_t
fBlaMka_neon(uint64x2_t x, uint64x2_t y)
{
    const uint32x2_t x_lo = vmovn_u64(x);       /* truncate to lower 32 bits */
    const uint32x2_t y_lo = vmovn_u64(y);
    const uint64x2_t z    = vmull_u32(x_lo, y_lo); /* 32×32 → 64 widening */
    return vaddq_u64(vaddq_u64(x, y), vaddq_u64(z, z));
}

/* -------------------------------------------------------------------------
 * Rotation helpers.
 *
 * rotr32: vrev64q_u32 — portable, available on ARMv7 NEON and AArch64.
 *
 * rotr24 / rotr16:
 *   AArch64 / Apple Silicon: vqtbl1q_u8 (one instruction).
 *   ARMv7+NEON:              shift+OR   (vqtbl1q_u8 is AArch64-only).
 *
 * rotr63: shift+XOR — portable on both targets.
 * ------------------------------------------------------------------------- */
static inline uint64x2_t
rotr64_32_neon(uint64x2_t x)
{
    return vreinterpretq_u64_u32(vrev64q_u32(vreinterpretq_u32_u64(x)));
}

static inline uint64x2_t
rotr64_24_neon(uint64x2_t x)
{
#if defined(__aarch64__) || defined(__arm64__)
    static const uint8_t rot24_tbl[16] = {
        3, 4, 5, 6, 7, 0, 1, 2,
        11, 12, 13, 14, 15, 8, 9, 10
    };
    const uint8x16_t tbl = vld1q_u8(rot24_tbl);
    return vreinterpretq_u64_u8(vqtbl1q_u8(vreinterpretq_u8_u64(x), tbl));
#else
    return vorrq_u64(vshrq_n_u64(x, 24), vshlq_n_u64(x, 40));
#endif
}

static inline uint64x2_t
rotr64_16_neon(uint64x2_t x)
{
#if defined(__aarch64__) || defined(__arm64__)
    static const uint8_t rot16_tbl[16] = {
        2, 3, 4, 5, 6, 7, 0, 1,
        10, 11, 12, 13, 14, 15, 8, 9
    };
    const uint8x16_t tbl = vld1q_u8(rot16_tbl);
    return vreinterpretq_u64_u8(vqtbl1q_u8(vreinterpretq_u8_u64(x), tbl));
#else
    return vorrq_u64(vshrq_n_u64(x, 16), vshlq_n_u64(x, 48));
#endif
}

static inline uint64x2_t
rotr64_63_neon(uint64x2_t x)
{
    return veorq_u64(vshrq_n_u64(x, 63), vaddq_u64(x, x));
}

/* -------------------------------------------------------------------------
 * G1_NEON / G2_NEON — first and second half of a Blake2b G step.
 *
 * Each macro operates on two parallel uint64x2_t columns simultaneously
 * (A0/A1, B0/B1, C0/C1, D0/D1), covering 16 consecutive uint64 words.
 * ------------------------------------------------------------------------- */
#define G1_NEON(A0, B0, C0, D0, A1, B1, C1, D1)   \
    do {                                           \
        A0 = fBlaMka_neon(A0, B0);                 \
        A1 = fBlaMka_neon(A1, B1);                 \
        D0 = veorq_u64(D0, A0);                    \
        D1 = veorq_u64(D1, A1);                    \
        D0 = rotr64_32_neon(D0);                   \
        D1 = rotr64_32_neon(D1);                   \
        C0 = fBlaMka_neon(C0, D0);                 \
        C1 = fBlaMka_neon(C1, D1);                 \
        B0 = veorq_u64(B0, C0);                    \
        B1 = veorq_u64(B1, C1);                    \
        B0 = rotr64_24_neon(B0);                   \
        B1 = rotr64_24_neon(B1);                   \
    } while ((void)0, 0)

#define G2_NEON(A0, B0, C0, D0, A1, B1, C1, D1)   \
    do {                                           \
        A0 = fBlaMka_neon(A0, B0);                 \
        A1 = fBlaMka_neon(A1, B1);                 \
        D0 = veorq_u64(D0, A0);                    \
        D1 = veorq_u64(D1, A1);                    \
        D0 = rotr64_16_neon(D0);                   \
        D1 = rotr64_16_neon(D1);                   \
        C0 = fBlaMka_neon(C0, D0);                 \
        C1 = fBlaMka_neon(C1, D1);                 \
        B0 = veorq_u64(B0, C0);                    \
        B1 = veorq_u64(B1, C1);                    \
        B0 = rotr64_63_neon(B0);                   \
        B1 = rotr64_63_neon(B1);                   \
    } while ((void)0, 0)

/* -------------------------------------------------------------------------
 * DIAGONALIZE_NEON / UNDIAGONALIZE_NEON
 *
 * The 8 registers A0,B0,C0,D0,A1,B1,C1,D1 cover a 4×4 matrix of uint64
 * values (each register holds 2 consecutive values in a uint64x2_t).
 *
 * Register layout: A0={v0,v1}, A1={v2,v3}, B0={v4,v5}, B1={v6,v7},
 *                  C0={v8,v9}, C1={v10,v11}, D0={v12,v13}, D1={v14,v15}
 *
 * DIAGONALIZE rotates B by +1, C by +2, D by +3 positions, producing
 * the operand order required by Blake2b's diagonal step:
 *   G(v0,v5,v10,v15), G(v1,v6,v11,v12),
 *   G(v2,v7,v8, v13), G(v3,v4,v9, v14)
 *
 * After the diagonal G passes, UNDIAGONALIZE reverses each rotation:
 *   B: undo +1 → rotate by +3  (= vextq from B1→B0 side)
 *   C: undo +2 → swap again
 *   D: undo +3 → rotate by +1  (= vextq from D0→D1 side)
 *
 * vextq_u8(a, b, 8): extract 16 bytes starting at byte 8 from concat(a,b)
 *   = { a[1], b[0] } when a,b are uint64x2_t pairs.
 *
 * DIAGONALIZE correctness (tracking D as example):
 *   D0={v12,v13}, D1={v14,v15}  →  want D0'={v15,v12}, D1'={v13,v14}
 *   _t0 = vextq(D0,D1) = {v13,v14}
 *   _t1 = vextq(D1,D0) = {v15,v12}
 *   D0=_t1={v15,v12} ✓   D1=_t0={v13,v14} ✓
 *
 * UNDIAGONALIZE correctness (tracking D):
 *   D0'={v15,v12}, D1'={v13,v14}  →  want D0={v12,v13}, D1={v14,v15}
 *   _t0 = vextq(D0',D1') = {D0'[1],D1'[0]} = {v12,v13}
 *   _t1 = vextq(D1',D0') = {D1'[1],D0'[0]} = {v14,v15}
 *   D0=_t0={v12,v13} ✓   D1=_t1={v14,v15} ✓
 *
 * Key: DIAGONALIZE uses (D0,D1)→_t1 for D0; UNDIAGONALIZE uses (D0',D1')→_t0
 * for D0. The vextq argument order is OPPOSITE between the two macros.
 * ------------------------------------------------------------------------- */
#define DIAGONALIZE_NEON(A0, B0, C0, D0, A1, B1, C1, D1)         \
    do {                                                          \
        uint64x2_t _t0, _t1;                                      \
        /* B: rotate by +1 */                                     \
        _t0 = vreinterpretq_u64_u8(                               \
            vextq_u8(vreinterpretq_u8_u64(B0),                    \
                     vreinterpretq_u8_u64(B1), 8));               \
        _t1 = vreinterpretq_u64_u8(                               \
            vextq_u8(vreinterpretq_u8_u64(B1),                    \
                     vreinterpretq_u8_u64(B0), 8));               \
        B0 = _t0;  B1 = _t1;                                      \
        /* C: rotate by +2 — swap the two registers entirely. */  \
        _t0 = C0;  C0 = C1;  C1 = _t0;                           \
        /* D: rotate by +3 (= rotate back by 1)               */  \
        _t0 = vreinterpretq_u64_u8(                               \
            vextq_u8(vreinterpretq_u8_u64(D0),                    \
                     vreinterpretq_u8_u64(D1), 8));               \
        _t1 = vreinterpretq_u64_u8(                               \
            vextq_u8(vreinterpretq_u8_u64(D1),                    \
                     vreinterpretq_u8_u64(D0), 8));               \
        D0 = _t1;  D1 = _t0;                                      \
    } while ((void)0, 0)

#define UNDIAGONALIZE_NEON(A0, B0, C0, D0, A1, B1, C1, D1)       \
    do {                                                          \
        uint64x2_t _t0, _t1;                                      \
        /* B: undo +1 (= rotate by +3) */                         \
        _t0 = vreinterpretq_u64_u8(                               \
            vextq_u8(vreinterpretq_u8_u64(B1),                    \
                     vreinterpretq_u8_u64(B0), 8));               \
        _t1 = vreinterpretq_u64_u8(                               \
            vextq_u8(vreinterpretq_u8_u64(B0),                    \
                     vreinterpretq_u8_u64(B1), 8));               \
        B0 = _t0;  B1 = _t1;                                      \
        /* C: undo +2 — swap again. */                            \
        _t0 = C0;  C0 = C1;  C1 = _t0;                           \
        /* D: undo +3 (= rotate by +1)                         */ \
        /* NOTE: vextq argument order is D0,D1 here (opposite  */ \
        /* of DIAGONALIZE which used D0,D1 → _t1 for D0).     */ \
        _t0 = vreinterpretq_u64_u8(                               \
            vextq_u8(vreinterpretq_u8_u64(D0),                    \
                     vreinterpretq_u8_u64(D1), 8));               \
        _t1 = vreinterpretq_u64_u8(                               \
            vextq_u8(vreinterpretq_u8_u64(D1),                    \
                     vreinterpretq_u8_u64(D0), 8));               \
        D0 = _t0;  D1 = _t1;                                      \
    } while ((void)0, 0)

/* -------------------------------------------------------------------------
 * BLAKE2_ROUND_NEON — full Blake2b round (G1 + G2 + diagonalize).
 *
 * Arguments: A0,A1,B0,B1,C0,C1,D0,D1  (8 uint64x2_t registers = 16 words)
 * ------------------------------------------------------------------------- */
#define BLAKE2_ROUND_NEON(A0, A1, B0, B1, C0, C1, D0, D1)        \
    do {                                                          \
        G1_NEON(A0, B0, C0, D0, A1, B1, C1, D1);                  \
        G2_NEON(A0, B0, C0, D0, A1, B1, C1, D1);                  \
        DIAGONALIZE_NEON(A0, B0, C0, D0, A1, B1, C1, D1);         \
        G1_NEON(A0, B0, C0, D0, A1, B1, C1, D1);                  \
        G2_NEON(A0, B0, C0, D0, A1, B1, C1, D1);                  \
        UNDIAGONALIZE_NEON(A0, B0, C0, D0, A1, B1, C1, D1);       \
    } while ((void)0, 0)

#endif // BITCOIN_CRYPTO_ARGON2D_BLAKE2_BLAMKA_ROUND_NEON_H
