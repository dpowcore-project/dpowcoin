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
 * Pure-C reference implementation.
 *
 * fill_segment_ref() is always compiled and always exported - it serves as the
 * safe fallback that opt.cpp (x86 dispatcher) defaults to before CPUID
 * detection upgrades to SSE2 / SSSE3 / AVX2 / AVX-512.
 *
 * On non-x86 targets (no HAVE_GETCPUID) this file also provides the public
 * fill_segment() and Argon2AutoDetectImpl() entry points, with an optional
 * NEON fast-path when ENABLE_ARGON2_NEON is defined (AArch64 or ARMv7+NEON).
 *
 * The non-x86 dispatcher uses the same function-pointer pattern as the x86
 * opt.cpp dispatcher: s_fill_segment defaults to fill_segment_ref and is
 * upgraded by Argon2AutoDetectImpl() when a faster implementation is
 * available.
 */

#include <compat/cpuid.h> /* defines HAVE_GETCPUID on x86 */
#include <crypto/argon2d/argon2_core.h>
#include <crypto/argon2d/argon2_types.h>
#include <crypto/argon2d/blake2/blamka-round-ref.h>

#include <cstdint>

/* -------------------------------------------------------------------------
 * Internal helpers - same as upstream reference implementation.
 * ------------------------------------------------------------------------- */

static void fill_block(const block *prev_block, const block *ref_block,
                       block *next_block, int with_xor)
{
    block blockR, block_tmp;
    unsigned i;

    copy_block(&blockR, ref_block);
    xor_block(&blockR, prev_block);
    copy_block(&block_tmp, &blockR);

    if (with_xor) {
        xor_block(&block_tmp, next_block);
    }

    /* Apply Blake2 on columns of 64-bit words */
    for (i = 0; i < 8; ++i) {
        BLAKE2_ROUND_NOMSG(
            blockR.v[16 * i],      blockR.v[16 * i + 1],  blockR.v[16 * i + 2],
            blockR.v[16 * i + 3],  blockR.v[16 * i + 4],  blockR.v[16 * i + 5],
            blockR.v[16 * i + 6],  blockR.v[16 * i + 7],  blockR.v[16 * i + 8],
            blockR.v[16 * i + 9],  blockR.v[16 * i + 10], blockR.v[16 * i + 11],
            blockR.v[16 * i + 12], blockR.v[16 * i + 13], blockR.v[16 * i + 14],
            blockR.v[16 * i + 15]);
    }

    /* Apply Blake2 on rows of 64-bit words */
    for (i = 0; i < 8; i++) {
        BLAKE2_ROUND_NOMSG(
            blockR.v[2 * i],       blockR.v[2 * i + 1],   blockR.v[2 * i + 16],
            blockR.v[2 * i + 17],  blockR.v[2 * i + 32],  blockR.v[2 * i + 33],
            blockR.v[2 * i + 48],  blockR.v[2 * i + 49],  blockR.v[2 * i + 64],
            blockR.v[2 * i + 65],  blockR.v[2 * i + 80],  blockR.v[2 * i + 81],
            blockR.v[2 * i + 96],  blockR.v[2 * i + 97],  blockR.v[2 * i + 112],
            blockR.v[2 * i + 113]);
    }

    copy_block(next_block, &block_tmp);
    xor_block(next_block, &blockR);
}

static void next_addresses(block *address_block, block *input_block,
                           const block *zero_block)
{
    input_block->v[6]++;
    fill_block(zero_block, input_block, address_block, 0);
    fill_block(zero_block, address_block, address_block, 0);
}

/* -------------------------------------------------------------------------
 * fill_segment_ref - always exported; used as fallback by the x86 dispatcher.
 * ------------------------------------------------------------------------- */
void fill_segment_ref(const argon2_instance_t *instance,
                      argon2_position_t position)
{
    block *ref_block = nullptr, *curr_block = nullptr;
    block address_block, input_block, zero_block;
    uint64_t pseudo_rand, ref_index, ref_lane;
    uint32_t prev_offset, curr_offset;
    uint32_t starting_index;
    uint32_t i;
    int data_independent_addressing;

    if (instance == nullptr) {
        return;
    }

    data_independent_addressing =
        (instance->type == Argon2_i) ||
        (instance->type == Argon2_id && (position.pass == 0) &&
         (position.slice < ARGON2_SYNC_POINTS / 2));

    if (data_independent_addressing) {
        init_block_value(&zero_block, 0);
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
            next_addresses(&address_block, &input_block, &zero_block);
        }
    }

    curr_offset = position.lane * instance->lane_length +
                  position.slice * instance->segment_length + starting_index;

    if (0 == curr_offset % instance->lane_length) {
        prev_offset = curr_offset + instance->lane_length - 1;
    } else {
        prev_offset = curr_offset - 1;
    }

    for (i = starting_index; i < instance->segment_length;
         ++i, ++curr_offset, ++prev_offset) {

        if (curr_offset % instance->lane_length == 1) {
            prev_offset = curr_offset - 1;
        }

        if (data_independent_addressing) {
            if (i % ARGON2_ADDRESSES_IN_BLOCK == 0) {
                next_addresses(&address_block, &input_block, &zero_block);
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

        ref_block =
            instance->memory + instance->lane_length * ref_lane + ref_index;
        curr_block = instance->memory + curr_offset;

        if (ARGON2_VERSION_10 == instance->version) {
            fill_block(instance->memory + prev_offset, ref_block, curr_block, 0);
        } else {
            fill_block(instance->memory + prev_offset, ref_block, curr_block,
                       position.pass != 0);
        }
    }
}

/* -------------------------------------------------------------------------
 * Non-x86 public entry points.
 *
 * On x86, fill_segment() and Argon2AutoDetectImpl() live in opt.cpp.
 *
 * On non-x86 (ARM, RISC-V, MIPS, …) this block provides both entry points.
 * The function pointer s_fill_segment defaults to fill_segment_ref and is
 * upgraded to fill_segment_neon when ENABLE_ARGON2_NEON is defined and the
 * caller passes a use_implementation value with bit 0x10 (USE_NEON) set.
 *
 * USE_NEON (0x10) is separate from the x86 bits (0x01-0x08).  Passing
 * USE_ALL (0x1F after adding USE_NEON) from context.cpp is safe: on x86
 * opt.cpp never checks bit 0x10; on ARM ref.cpp never checks bits 0x01-0x08.
 * ------------------------------------------------------------------------- */
#if !defined(HAVE_GETCPUID)

/* Forward declaration - defined in argon2_opt_neon.cpp when compiled. */
#if defined(ENABLE_ARGON2_NEON)
void fill_segment_neon(const argon2_instance_t *instance,
                       argon2_position_t position);
#endif

static void (*s_fill_segment)(const argon2_instance_t *,
                              argon2_position_t) = fill_segment_ref;

void fill_segment(const argon2_instance_t *instance,
                  argon2_position_t position)
{
    s_fill_segment(instance, position);
}

const char *Argon2AutoDetectImpl(uint8_t use_implementation)
{
    s_fill_segment = fill_segment_ref;

#if defined(ENABLE_ARGON2_NEON)
    if (use_implementation & 0x10) { /* argon2_implementation::USE_NEON */
        s_fill_segment = fill_segment_neon;
        return "neon";
    }
#endif

    (void)use_implementation;
    return "reference";
}

#endif /* !HAVE_GETCPUID */
