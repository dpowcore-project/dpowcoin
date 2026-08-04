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
 * SSSE3 fill_segment - compiled separately with ${ARGON2_SSSE3_CXXFLAGS}
 * (-mssse3).
 *
 * Structurally identical to argon2_opt_sse2.cpp; the speedup comes from
 * blamka-round-ssse3.h which defines all rotation and diagonalize helpers
 * unconditionally using SSSE3 instructions:
 *
 *   rotr24: _mm_shuffle_epi8(x, r24)     vs. shift+or in SSE2
 *   rotr16: _mm_shuffle_epi8(x, r16)     vs. shift+or in SSE2
 *   rotr32: _mm_shuffle_epi32(...)        (same cost both paths)
 *   diag:   _mm_alignr_epi8(...)          vs. unpack pair in SSE2
 *
 * WHY NOT blamka-round-sse2.h:
 * That header has #if defined(__SSSE3__) which silently activates on any
 * build host with SSSE3 (all CPUs since ~2008), making the SSE2 translation
 * unit identical to SSSE3 when -march is broad. Using the self-contained
 * blamka-round-ssse3.h guarantees clean tier separation regardless of host.
 *
 * Target machines: Sandy Bridge, Ivy Bridge (SSSE3, no AVX2).
 * On Haswell+ the AVX2 path wins; this path is never selected there.
 *
 * Bit in use_implementation: 0x02 (see argon2_opt.cpp).
 */

#ifdef ENABLE_ARGON2_SSSE3

#include <compat/cpuid.h>
#include <crypto/argon2d/argon2_types.h>
#include <immintrin.h>

#if defined(HAVE_GETCPUID)

#include <crypto/argon2d/argon2_core.h>
#include <crypto/argon2d/blake2/blamka-round-ssse3.h> /* self-contained SSSE3 */

#include <cstdint>
#include <cstring>

/* -------------------------------------------------------------------------
 * fill_block - SSSE3 / __m128i version.
 *
 * state[] holds ARGON2_OWORDS_IN_BLOCK (64) __m128i elements covering the
 * full 1024-byte block (each __m128i = 2 × 64-bit words).
 * BLAKE2_ROUND_SSSE3 operates on 8 __m128i values at a time.
 * ------------------------------------------------------------------------- */
static void fill_block(__m128i *state, const block *ref_block,
                       block *next_block, int with_xor)
{
    __m128i block_XY[ARGON2_OWORDS_IN_BLOCK];
    unsigned int i;

    if (with_xor) {
        for (i = 0; i < ARGON2_OWORDS_IN_BLOCK; i++) {
            state[i] = _mm_xor_si128(
                state[i], _mm_loadu_si128((const __m128i *)ref_block->v + i));
            block_XY[i] = _mm_xor_si128(
                state[i], _mm_loadu_si128((const __m128i *)next_block->v + i));
        }
    } else {
        for (i = 0; i < ARGON2_OWORDS_IN_BLOCK; i++) {
            block_XY[i] = state[i] = _mm_xor_si128(
                state[i], _mm_loadu_si128((const __m128i *)ref_block->v + i));
        }
    }

    /* Column pass - 8 iterations × 8 elements */
    for (i = 0; i < 8; ++i) {
        BLAKE2_ROUND_SSSE3(
            state[8 * i + 0], state[8 * i + 1], state[8 * i + 2], state[8 * i + 3],
            state[8 * i + 4], state[8 * i + 5], state[8 * i + 6], state[8 * i + 7]);
    }

    /* Row pass - 8 iterations × 8 elements (strided) */
    for (i = 0; i < 8; ++i) {
        BLAKE2_ROUND_SSSE3(
            state[8 * 0 + i], state[8 * 1 + i], state[8 * 2 + i], state[8 * 3 + i],
            state[8 * 4 + i], state[8 * 5 + i], state[8 * 6 + i], state[8 * 7 + i]);
    }

    for (i = 0; i < ARGON2_OWORDS_IN_BLOCK; i++) {
        state[i] = _mm_xor_si128(state[i], block_XY[i]);
        _mm_storeu_si128((__m128i *)next_block->v + i, state[i]);
    }
}

static void next_addresses(block *address_block, block *input_block)
{
    __m128i zero_block[ARGON2_OWORDS_IN_BLOCK];
    __m128i zero2_block[ARGON2_OWORDS_IN_BLOCK];
    memset(zero_block,  0, sizeof(zero_block));
    memset(zero2_block, 0, sizeof(zero2_block));
    input_block->v[6]++;
    fill_block(zero_block,  input_block,   address_block, 0);
    fill_block(zero2_block, address_block, address_block, 0);
}

/* -------------------------------------------------------------------------
 * fill_segment_ssse3 - exported; registered by Argon2AutoDetectImpl (opt.cpp).
 * Slot: preferred over SSE2, superseded by AVX2 when available.
 * ------------------------------------------------------------------------- */
void fill_segment_ssse3(const argon2_instance_t *instance,
                        argon2_position_t position)
{
    block *ref_block = nullptr, *curr_block = nullptr;
    block address_block, input_block;
    uint64_t pseudo_rand, ref_index, ref_lane;
    uint32_t prev_offset, curr_offset;
    uint32_t starting_index, i;
    __m128i state[ARGON2_OWORDS_IN_BLOCK];
    int data_independent_addressing;

    if (instance == nullptr) {
        return;
    }

    data_independent_addressing =
        (instance->type == Argon2_i) ||
        (instance->type == Argon2_id && (position.pass == 0) &&
         (position.slice < ARGON2_SYNC_POINTS / 2));

    if (data_independent_addressing) {
        init_block_value(&input_block, 0);
        input_block.v[0] = position.pass;
        input_block.v[1] = position.lane;
        input_block.v[2] = position.slice;
        input_block.v[3] = instance->memory_blocks;
        input_block.v[4] = instance->passes;
        input_block.v[5] = instance->type;
    }

    starting_index = 0;

    if ((0 == position.pass) && (0 == position.slice)) {
        starting_index = 2;
        if (data_independent_addressing) {
            next_addresses(&address_block, &input_block);
        }
    }

    curr_offset = position.lane * instance->lane_length +
                  position.slice * instance->segment_length + starting_index;

    if (0 == curr_offset % instance->lane_length) {
        prev_offset = curr_offset + instance->lane_length - 1;
    } else {
        prev_offset = curr_offset - 1;
    }

    memcpy(state, ((instance->memory + prev_offset)->v), ARGON2_BLOCK_SIZE);

    for (i = starting_index; i < instance->segment_length;
         ++i, ++curr_offset, ++prev_offset) {

        if (curr_offset % instance->lane_length == 1) {
            prev_offset = curr_offset - 1;
        }

        if (data_independent_addressing) {
            if (i % ARGON2_ADDRESSES_IN_BLOCK == 0) {
                next_addresses(&address_block, &input_block);
            }
            pseudo_rand = address_block.v[i % ARGON2_ADDRESSES_IN_BLOCK];
        } else {
            pseudo_rand = instance->memory[prev_offset].v[0];
        }

        ref_lane = ((pseudo_rand >> 32)) % instance->lanes;
        if ((position.pass == 0) && (position.slice == 0)) {
            ref_lane = position.lane;
        }

        position.index = i;
        ref_index = index_alpha(instance, &position, pseudo_rand & 0xFFFFFFFF,
                                ref_lane == position.lane);

        ref_block  = instance->memory + instance->lane_length * ref_lane + ref_index;
        curr_block = instance->memory + curr_offset;

        if (ARGON2_VERSION_10 == instance->version) {
            fill_block(state, ref_block, curr_block, 0);
        } else {
            fill_block(state, ref_block, curr_block, position.pass != 0);
        }
    }
}

#endif /* HAVE_GETCPUID */
#endif /* ENABLE_ARGON2_SSSE3 */
