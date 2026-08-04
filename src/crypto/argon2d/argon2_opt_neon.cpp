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
 * NEON fill_segment - compiled separately with ${ARGON2_NEON_CXXFLAGS}.
 *
 * No flags on AArch64 (NEON is the mandatory baseline).
 * -mfpu=neon on ARMv7 targets.
 *
 * Self-contained: all Blake2b round primitives come from
 * <crypto/argon2d/blake2/blamka-round-neon.h>, which contains only
 * portable NEON intrinsics available on both AArch64 and ARMv7+NEON.
 *
 * Key differences from the libsodium-derived argon2-fill-block-neon.c:
 *
 *   1. Uses instance->memory (not instance->region->memory) - matches
 *      the Dpowcoin argon2_instance_t layout used by all other ISA variants.
 *
 *   2. Uses const block * / block * API (same as argon2_ref.cpp and
 *      argon2_opt_avx2.cpp) rather than raw uint8_t* pointers.
 *
 *   3. Single fill_block() with int with_xor flag, following AVX2 pattern.
 *
 *   4. vqtbl1q_u8 (AArch64-only) replaced by portable shift+OR in
 *      blamka-round-neon.h - file now compiles on ARMv7+NEON as well.
 *
 * Structure mirrors argon2_opt_avx2.cpp:
 *   - ARGON2_OWORDS_IN_BLOCK (64) uint64x2_t elements = 1024-byte block.
 *   - state[] maintained across segment loop; loaded once from prev_block.
 *   - data_independent_addressing handled identically to AVX2 variant.
 */

#ifdef ENABLE_ARGON2_NEON

#include <cstdint>
#include <cstring>
#include <arm_neon.h>

#include <crypto/argon2d/argon2.h>
#include <crypto/argon2d/argon2_core.h>
#include <crypto/argon2d/blake2/blake2.h>
#include <crypto/argon2d/blake2/blamka-round-neon.h>

/* -------------------------------------------------------------------------
 * fill_block - NEON version.
 *
 * state[ARGON2_OWORDS_IN_BLOCK] carries the accumulated NEON state across
 * calls within a segment; it is initialised once from prev_block before
 * the inner loop in fill_segment_neon.
 *
 * ARGON2_OWORDS_IN_BLOCK = ARGON2_BLOCK_SIZE / 16 = 1024 / 16 = 64.
 * Each uint64x2_t holds 2 consecutive uint64 words from block.v[].
 *
 * Column pass: 8 rounds, each covering 8 consecutive uint64x2_t elements
 *   = 16 uint64 words = one 4×4 Blake2 column group.
 * Row pass:    8 rounds with stride 8 across the full block.
 *
 * Aliasing note for next_addresses: ref_block == next_block is safe because
 * all reads from ref_block (first loop) complete before any writes to
 * next_block (last loop).
 * ------------------------------------------------------------------------- */
static void
fill_block(uint64x2_t *state, const block *ref_block, block *next_block,
           int with_xor)
{
    uint64x2_t block_XY[ARGON2_OWORDS_IN_BLOCK];
    unsigned int i;

    if (with_xor) {
        for (i = 0; i < ARGON2_OWORDS_IN_BLOCK; i++) {
            state[i] = veorq_u64(
                state[i],
                vld1q_u64((const uint64_t *)(&ref_block->v[2 * i])));
            block_XY[i] = veorq_u64(
                state[i],
                vld1q_u64((const uint64_t *)(&next_block->v[2 * i])));
        }
    } else {
        for (i = 0; i < ARGON2_OWORDS_IN_BLOCK; i++) {
            block_XY[i] = state[i] = veorq_u64(
                state[i],
                vld1q_u64((const uint64_t *)(&ref_block->v[2 * i])));
        }
    }

    /* Column pass - 8 iterations, each covering 8 uint64x2_t = 16 words */
    for (i = 0; i < 8; ++i) {
        BLAKE2_ROUND_NEON(
            state[8 * i + 0], state[8 * i + 1],
            state[8 * i + 2], state[8 * i + 3],
            state[8 * i + 4], state[8 * i + 5],
            state[8 * i + 6], state[8 * i + 7]);
    }

    /* Row pass - 8 iterations, stride 8 */
    for (i = 0; i < 8; ++i) {
        BLAKE2_ROUND_NEON(
            state[8 * 0 + i], state[8 * 1 + i],
            state[8 * 2 + i], state[8 * 3 + i],
            state[8 * 4 + i], state[8 * 5 + i],
            state[8 * 6 + i], state[8 * 7 + i]);
    }

    for (i = 0; i < ARGON2_OWORDS_IN_BLOCK; i++) {
        state[i] = veorq_u64(state[i], block_XY[i]);
        vst1q_u64((uint64_t *)(&next_block->v[2 * i]), state[i]);
    }
}

/* -------------------------------------------------------------------------
 * next_addresses - generate ARGON2_ADDRESSES_IN_BLOCK pseudo-random
 * addresses for data-independent addressing (Argon2i / Argon2id pass 0).
 *
 * Mirrors AVX2 next_addresses exactly:
 *   two fill_block calls with zeroed state and with_xor=0.
 * ------------------------------------------------------------------------- */
static void
next_addresses(block *address_block, block *input_block)
{
    uint64x2_t zero_block[ARGON2_OWORDS_IN_BLOCK];
    uint64x2_t zero2_block[ARGON2_OWORDS_IN_BLOCK];
    memset(zero_block,  0, sizeof(zero_block));
    memset(zero2_block, 0, sizeof(zero2_block));
    input_block->v[6]++;
    fill_block(zero_block,  input_block,   address_block, 0);
    fill_block(zero2_block, address_block, address_block, 0);
}

/* -------------------------------------------------------------------------
 * fill_segment_neon - exported symbol; registered as the active
 * fill_segment implementation by Argon2AutoDetectImpl (argon2_ref.cpp
 * non-x86 path) when ENABLE_ARGON2_NEON is defined.
 *
 * Identical control flow to fill_segment_avx2; only the state vector
 * type and fill_block internals differ.
 * ------------------------------------------------------------------------- */
void
fill_segment_neon(const argon2_instance_t *instance,
                  argon2_position_t position)
{
    block *ref_block = nullptr, *curr_block = nullptr;
    block address_block, input_block;
    uint64_t pseudo_rand, ref_index, ref_lane;
    uint32_t prev_offset, curr_offset;
    uint32_t starting_index, i;
    uint64x2_t state[ARGON2_OWORDS_IN_BLOCK];
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

#endif /* ENABLE_ARGON2_NEON */
