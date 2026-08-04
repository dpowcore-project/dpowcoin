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
 * AVX2 fill_segment - compiled separately with ${ARGON2_AVX2_CXXFLAGS} (-mavx2).
 *
 * Self-contained: all Blake2b round primitives come from blamka-round-avx2.h,
 * which contains only __m256i code with no #if __AVX512F__ dispatch.
 * The old blamka-round-opt.h is NOT included here.
 *
 * Future <>-migration: "blake2/blamka-round-avx2.h"
 *                   → <crypto/argon2d/blake2/blamka-round-avx2.h>
 */

#ifdef ENABLE_ARGON2_AVX2

#include <crypto/argon2d/argon2_core.h>
#include <crypto/argon2d/argon2_types.h>
#include <crypto/argon2d/blake2/blamka-round-avx2.h> /* __m256i, BLAKE2_ROUND_1/2_AVX2 */
#include <immintrin.h>

#include <cstdint>
#include <cstring>

/* -------------------------------------------------------------------------
 * fill_block - AVX2 / __m256i version.
 *
 * state[] holds ARGON2_HWORDS_IN_BLOCK (32) __m256i elements, covering the
 * full 1024-byte Argon2 block (each __m256i = 4 × 64-bit words).
 * ------------------------------------------------------------------------- */
static void fill_block(__m256i *state, const block *ref_block,
                       block *next_block, int with_xor)
{
    __m256i block_XY[ARGON2_HWORDS_IN_BLOCK];
    unsigned int i;

    if (with_xor) {
        for (i = 0; i < ARGON2_HWORDS_IN_BLOCK; i++) {
            state[i] = _mm256_xor_si256(
                state[i], _mm256_loadu_si256((const __m256i *)ref_block->v + i));
            block_XY[i] = _mm256_xor_si256(
                state[i], _mm256_loadu_si256((const __m256i *)next_block->v + i));
        }
    } else {
        for (i = 0; i < ARGON2_HWORDS_IN_BLOCK; i++) {
            block_XY[i] = state[i] = _mm256_xor_si256(
                state[i], _mm256_loadu_si256((const __m256i *)ref_block->v + i));
        }
    }

    /* Column pass - 4 iterations */
    for (i = 0; i < 4; ++i) {
        BLAKE2_ROUND_1_AVX2(
            state[8 * i + 0], state[8 * i + 4], state[8 * i + 1], state[8 * i + 5],
            state[8 * i + 2], state[8 * i + 6], state[8 * i + 3], state[8 * i + 7]);
    }

    /* Row pass - 4 iterations */
    for (i = 0; i < 4; ++i) {
        BLAKE2_ROUND_2_AVX2(
            state[ 0 + i], state[ 4 + i], state[ 8 + i], state[12 + i],
            state[16 + i], state[20 + i], state[24 + i], state[28 + i]);
    }

    for (i = 0; i < ARGON2_HWORDS_IN_BLOCK; i++) {
        state[i] = _mm256_xor_si256(state[i], block_XY[i]);
        _mm256_storeu_si256((__m256i *)next_block->v + i, state[i]);
    }
}

static void next_addresses(block *address_block, block *input_block)
{
    __m256i zero_block[ARGON2_HWORDS_IN_BLOCK];
    __m256i zero2_block[ARGON2_HWORDS_IN_BLOCK];
    memset(zero_block,  0, sizeof(zero_block));
    memset(zero2_block, 0, sizeof(zero2_block));
    input_block->v[6]++;
    fill_block(zero_block,  input_block,   address_block, 0);
    fill_block(zero2_block, address_block, address_block, 0);
}

/* -------------------------------------------------------------------------
 * fill_segment_avx2 - exported; registered by Argon2AutoDetectImpl (opt.cpp).
 * ------------------------------------------------------------------------- */
void fill_segment_avx2(const argon2_instance_t *instance,
                       argon2_position_t position)
{
    block *ref_block = nullptr, *curr_block = nullptr;
    block address_block, input_block;
    uint64_t pseudo_rand, ref_index, ref_lane;
    uint32_t prev_offset, curr_offset;
    uint32_t starting_index, i;
    __m256i state[ARGON2_HWORDS_IN_BLOCK];
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

#endif /* ENABLE_ARGON2_AVX2 */
