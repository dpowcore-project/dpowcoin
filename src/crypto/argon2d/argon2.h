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

#ifndef BITCOIN_CRYPTO_ARGON2D_ARGON2_H
#define BITCOIN_CRYPTO_ARGON2D_ARGON2_H

#include <crypto/argon2d/argon2_types.h>

#include <cstddef>
#include <cstdint>
#include <string>

/*
 * Function that performs memory-hard hashing with certain degree of parallelism
 * @param  context  Pointer to the Argon2 internal structure
 * @return Error code if there is a failure, otherwise ARGON2_OK
 */
ARGON2_PUBLIC int argon2_ctx(argon2_context *context, argon2_type type);

/**
 * Hashes a password with Argon2i, producing an encoded hash
 * @param t_cost Number of iterations
 * @param m_cost Sets memory usage to m_cost kibibytes
 * @param parallelism Number of threads and compute lanes
 * @param pwd Pointer to password
 * @param pwdlen Password size in bytes
 * @param salt Pointer to salt
 * @param saltlen Salt size in bytes
 * @param hashlen Desired length of the hash in bytes
 * @param encoded Buffer where to write the encoded hash
 * @param encodedlen Size of the buffer (thus max size of the encoded hash)
 * @pre   Different parallelism levels will give different results
 * @pre   Returns ARGON2_OK if successful
 */
ARGON2_PUBLIC int argon2i_hash_encoded(uint32_t t_cost,
                                       uint32_t m_cost,
                                       uint32_t parallelism,
                                       const void *pwd, size_t pwdlen,
                                       const void *salt, size_t saltlen,
                                       size_t hashlen, char *encoded,
                                       size_t encodedlen);

/**
 * Hashes a password with Argon2i, producing a raw hash at @hash
 * @param t_cost Number of iterations
 * @param m_cost Sets memory usage to m_cost kibibytes
 * @param parallelism Number of threads and compute lanes
 * @param pwd Pointer to password
 * @param pwdlen Password size in bytes
 * @param salt Pointer to salt
 * @param saltlen Salt size in bytes
 * @param hash Buffer where to write the raw hash - updated by the function
 * @param hashlen Desired length of the hash in bytes
 * @pre   Different parallelism levels will give different results
 * @pre   Returns ARGON2_OK if successful
 */
ARGON2_PUBLIC int argon2i_hash_raw(uint32_t t_cost, uint32_t m_cost,
                                   uint32_t parallelism, const void *pwd,
                                   size_t pwdlen, const void *salt,
                                   size_t saltlen, void *hash,
                                   size_t hashlen);

ARGON2_PUBLIC int argon2d_hash_encoded(uint32_t t_cost,
                                       uint32_t m_cost,
                                       uint32_t parallelism,
                                       const void *pwd, size_t pwdlen,
                                       const void *salt, size_t saltlen,
                                       size_t hashlen, char *encoded,
                                       size_t encodedlen);

ARGON2_PUBLIC int argon2d_hash_raw(uint32_t t_cost, uint32_t m_cost,
                                   uint32_t parallelism, const void *pwd,
                                   size_t pwdlen, const void *salt,
                                   size_t saltlen, void *hash,
                                   size_t hashlen);

ARGON2_PUBLIC int argon2id_hash_encoded(uint32_t t_cost,
                                        uint32_t m_cost,
                                        uint32_t parallelism,
                                        const void *pwd, size_t pwdlen,
                                        const void *salt, size_t saltlen,
                                        size_t hashlen, char *encoded,
                                        size_t encodedlen);

ARGON2_PUBLIC int argon2id_hash_raw(uint32_t t_cost,
                                    uint32_t m_cost,
                                    uint32_t parallelism, const void *pwd,
                                    size_t pwdlen, const void *salt,
                                    size_t saltlen, void *hash,
                                    size_t hashlen);

/* generic function underlying the above ones */
ARGON2_PUBLIC int argon2_hash(uint32_t t_cost, uint32_t m_cost,
                              uint32_t parallelism, const void *pwd,
                              size_t pwdlen, const void *salt,
                              size_t saltlen, void *hash,
                              size_t hashlen, char *encoded,
                              size_t encodedlen, argon2_type type,
                              uint32_t version);

/**
 * Verifies a password against an encoded string
 * Encoded string is restricted as in validate_inputs()
 * @param encoded String encoding parameters, salt, hash
 * @param pwd Pointer to password
 * @pre   Returns ARGON2_OK if successful
 */
ARGON2_PUBLIC int argon2i_verify(const char *encoded, const void *pwd,
                                 size_t pwdlen);

ARGON2_PUBLIC int argon2d_verify(const char *encoded, const void *pwd,
                                 size_t pwdlen);

ARGON2_PUBLIC int argon2id_verify(const char *encoded, const void *pwd,
                                  size_t pwdlen);

/* generic function underlying the above ones */
ARGON2_PUBLIC int argon2_verify(const char *encoded, const void *pwd,
                                size_t pwdlen, argon2_type type);

/**
 * Argon2d: Version of Argon2 that picks memory blocks depending
 * on the password and salt. Only for side-channel-free
 * environment!!
 *****
 * @param  context  Pointer to current Argon2 context
 * @return  Zero if successful, a non zero error code otherwise
 */
ARGON2_PUBLIC int argon2d_ctx(argon2_context *context);

/**
 * Argon2i: Version of Argon2 that picks memory blocks
 * independent on the password and salt. Good for side-channels,
 * but worse w.r.t. tradeoff attacks if only one pass is used.
 *****
 * @param  context  Pointer to current Argon2 context
 * @return  Zero if successful, a non zero error code otherwise
 */
ARGON2_PUBLIC int argon2i_ctx(argon2_context *context);

/**
 * Argon2id: Version of Argon2 where the first half-pass over memory is
 * password-independent, the rest are password-dependent (on the password and
 * salt). OK against side channels (they reduce to 1/2-pass Argon2i), and
 * better with w.r.t. tradeoff attacks (similar to Argon2d).
 *****
 * @param  context  Pointer to current Argon2 context
 * @return  Zero if successful, a non zero error code otherwise
 */
ARGON2_PUBLIC int argon2id_ctx(argon2_context *context);

/**
 * Verify if a given password is correct for Argon2d hashing
 * @param  context  Pointer to current Argon2 context
 * @param  hash  The password hash to verify. The length of the hash is
 * specified by the context outlen member
 * @return  Zero if successful, a non zero error code otherwise
 */
ARGON2_PUBLIC int argon2d_verify_ctx(argon2_context *context, const char *hash);

/**
 * Verify if a given password is correct for Argon2i hashing
 * @param  context  Pointer to current Argon2 context
 * @param  hash  The password hash to verify. The length of the hash is
 * specified by the context outlen member
 * @return  Zero if successful, a non zero error code otherwise
 */
ARGON2_PUBLIC int argon2i_verify_ctx(argon2_context *context, const char *hash);

/**
 * Verify if a given password is correct for Argon2id hashing
 * @param  context  Pointer to current Argon2 context
 * @param  hash  The password hash to verify. The length of the hash is
 * specified by the context outlen member
 * @return  Zero if successful, a non zero error code otherwise
 */
ARGON2_PUBLIC int argon2id_verify_ctx(argon2_context *context,
                                      const char *hash);

/* generic function underlying the above ones */
ARGON2_PUBLIC int argon2_verify_ctx(argon2_context *context, const char *hash,
                                    argon2_type type);

/**
 * Get the associated error message for given error code
 * @return  The error message associated with the given error code
 */
ARGON2_PUBLIC const char *argon2_error_message(int error_code);

/**
 * Returns the encoded hash length for the given input parameters
 * @param t_cost  Number of iterations
 * @param m_cost  Memory usage in kibibytes
 * @param parallelism  Number of threads; used to compute lanes
 * @param saltlen  Salt size in bytes
 * @param hashlen  Hash size in bytes
 * @param type The argon2_type that we want the encoded length for
 * @return  The encoded hash length in bytes
 */
ARGON2_PUBLIC size_t argon2_encodedlen(uint32_t t_cost, uint32_t m_cost,
                                       uint32_t parallelism, uint32_t saltlen,
                                       uint32_t hashlen, argon2_type type);

namespace argon2_implementation {
enum UseImplementation : uint8_t {
    STANDARD   = 0,
    USE_SSE2   = 1 << 0, /* 0x01 - x86: SSE2 baseline                 */
    USE_SSSE3  = 1 << 1, /* 0x02 - x86: SSSE3 (Sandy/Ivy Bridge)      */
    USE_AVX2   = 1 << 2, /* 0x04 - x86: AVX2 (Haswell+)               */
    USE_AVX512 = 1 << 3, /* 0x08 - x86: AVX-512F (Skylake-X+)         */
    USE_NEON   = 1 << 4, /* 0x10 - ARM: NEON (AArch64 / ARMv7+NEON)   */
    /* Bits 0x01-0x08 are only tested on x86 (opt.cpp / HAVE_GETCPUID).
     * Bit 0x10 is only tested on non-x86 (ref.cpp / !HAVE_GETCPUID).
     * Passing USE_ALL on any architecture is therefore always safe. */
    USE_ALL    = USE_SSE2 | USE_SSSE3 | USE_AVX2 | USE_AVX512 | USE_NEON,
};
}

const char *Argon2AutoDetectImpl(uint8_t use_implementation);

inline std::string Argon2AutoDetect(
    argon2_implementation::UseImplementation use_implementation =
        argon2_implementation::USE_ALL)
{
    return std::string(Argon2AutoDetectImpl(static_cast<uint8_t>(use_implementation)));
}

#endif // BITCOIN_CRYPTO_ARGON2D_ARGON2_H
