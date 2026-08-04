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
 * x86 runtime ISA selector for Argon2 fill_segment.
 *
 * This file contains ONLY the function-pointer dispatcher and CPUID detection.
 * No SIMD code lives here - each ISA variant is its own translation unit:
 *
 *   argon2_opt_sse2.cpp   - SSE2 baseline  (compiled with -msse2)
 *   argon2_opt_ssse3.cpp  - SSSE3          (compiled with -mssse3)
 *   argon2_opt_avx2.cpp   - AVX2           (compiled with -mavx2)
 *   argon2_opt_avx512.cpp - AVX-512F       (compiled with -mavx512f)
 *   argon2_ref.cpp        - pure-C reference, always compiled, always exported as
 *                    fill_segment_ref(); used as the safe default before CPUID
 *                    detection runs, and on i686 without SSE2.
 *
 * Structure mirrors sha256.cpp:
 *   - HAVE_GETCPUID (from cpuid.h) is the x86 guard.
 *   - ENABLE_ARGON2_SSE2 / SSSE3 / AVX2 / AVX512 gate the separately-compiled variants.
 *   - Non-x86 fill_segment() and Argon2AutoDetectImpl() are in ref.cpp.
 */

#include <compat/cpuid.h>

#if defined(HAVE_GETCPUID)

#include <cstdint>

#include <crypto/argon2d/argon2.h>
#include <crypto/argon2d/argon2_core.h>

/* -------------------------------------------------------------------------
 * Forward declarations for all ISA variants.
 * Each symbol is defined in its own translation unit.
 * ------------------------------------------------------------------------- */

/* Always available - pure-C reference (ref.cpp) */
void fill_segment_ref(const argon2_instance_t *instance,
                      argon2_position_t position);

#if defined(ENABLE_ARGON2_SSE2)
void fill_segment_sse2(const argon2_instance_t *instance,
                       argon2_position_t position);
#endif

#if defined(ENABLE_ARGON2_SSSE3)
void fill_segment_ssse3(const argon2_instance_t *instance,
                        argon2_position_t position);
#endif

#if defined(ENABLE_ARGON2_AVX2)
void fill_segment_avx2(const argon2_instance_t *instance,
                       argon2_position_t position);
#endif

#if defined(ENABLE_ARGON2_AVX512)
void fill_segment_avx512(const argon2_instance_t *instance,
                         argon2_position_t position);
#endif

/* -------------------------------------------------------------------------
 * AVX OS-enable checks - same helpers as sha256.cpp.
 * ------------------------------------------------------------------------- */
 #if defined(ENABLE_ARGON2_AVX2) || defined(ENABLE_ARGON2_AVX512)
static int AVXEnabled(void)
{
    uint32_t a, d;
    __asm__("xgetbv" : "=a"(a), "=d"(d) : "c"(0));
    return (a & 6) == 6;
}
#endif

#if defined(ENABLE_ARGON2_AVX512)
static int AVX512Enabled(void)
{
    uint32_t a, d;
    __asm__("xgetbv" : "=a"(a), "=d"(d) : "c"(0));
    return (a & 0xE6) == 0xE6;
}
#endif

/* -------------------------------------------------------------------------
 * argon2_fill_segment - global function pointer.
 *
 * Default: fill_segment_ref (pure-C, always safe).
 * Argon2AutoDetectImpl() upgrades this to SSE2, AVX2, or AVX-512 when
 * the CPU and OS support the required feature set.
 * ------------------------------------------------------------------------- */
void (*argon2_fill_segment)(const argon2_instance_t *instance,
                            argon2_position_t position) = fill_segment_ref;

/* Public entry point called by argon2_core.c - routes through the function pointer. */
void fill_segment(const argon2_instance_t *instance,
                  argon2_position_t position)
{
    argon2_fill_segment(instance, position);
}

/* -------------------------------------------------------------------------
 * Argon2AutoDetectImpl - x86 implementation.
 * Called once at startup (kernel/context.cpp) via Argon2AutoDetect().
 * ------------------------------------------------------------------------- */
const char *Argon2AutoDetectImpl(uint8_t use_implementation)
{
    const char *ret = "reference";
    argon2_fill_segment = fill_segment_ref;

    {
        uint32_t eax, ebx, ecx, edx;
        int have_xsave, have_avx, enabled_avx;
        int have_avx2, have_avx512f;
        int have_sse2, have_ssse3;

        GetCPUID(1, 0, eax, ebx, ecx, edx);
        have_xsave  = (ecx >> 27) & 1;
        have_avx    = (ecx >> 28) & 1;
        have_sse2   = (edx >> 26) & 1;
        have_ssse3  = (ecx >> 9)  & 1;
        enabled_avx = 0;
#if defined(ENABLE_ARGON2_AVX2) || defined(ENABLE_ARGON2_AVX512)
        if (have_xsave && have_avx) {
            enabled_avx = AVXEnabled();
        }
#endif

        GetCPUID(7, 0, eax, ebx, ecx, edx);
        have_avx2    = (ebx >> 5)  & 1;
        have_avx512f = (ebx >> 16) & 1;

#if defined(ENABLE_ARGON2_AVX512)
        if ((use_implementation & 0x08) &&
            have_avx512f && have_xsave && AVX512Enabled()) {
            argon2_fill_segment = fill_segment_avx512;
            ret = "avx512";
        } else
#endif
#if defined(ENABLE_ARGON2_AVX2)
        if ((use_implementation & 0x04) &&
            have_avx2 && have_avx && enabled_avx) {
            argon2_fill_segment = fill_segment_avx2;
            ret = "avx2";
        } else
#endif
#if defined(ENABLE_ARGON2_SSSE3)
        if ((use_implementation & 0x02) && have_ssse3) {
            argon2_fill_segment = fill_segment_ssse3;
            ret = "ssse3";
        } else
#endif
#if defined(ENABLE_ARGON2_SSE2)
        if ((use_implementation & 0x01) && have_sse2) {
            argon2_fill_segment = fill_segment_sse2;
            ret = "sse2";
        } else
#endif
        {
            /* STANDARD=0 or no matching tier compiled - stay on reference */
            (void)have_xsave;
            (void)have_avx; (void)have_avx2; (void)have_avx512f;
            (void)have_ssse3; (void)have_sse2; (void)enabled_avx;
        }
    }

    return ret;
}

#endif /* HAVE_GETCPUID */
